// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/authenticator_request_window.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/fido/enclave/metrics.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/gfx/geometry/rect.h"

namespace {

const char kGpmPinResetReauthUrl[] =
    "https://passwords.google.com/encryption/pin/reset";

// The kdi parameter here was generated from the following protobuf:
//
// {
//   operation: RETRIEVAL
//   retrieval_inputs: {
//     security_domain_name: "hw_protected"
//   }
// }
//
// And then converted to bytes with:
//
// % gqui --outfile=rawproto:/tmp/out.pb from textproto:/tmp/input \
//       proto gaia_frontend.ClientDecryptableKeyDataInputs
//
// Then the contents of `/tmp/out.pb` need to be base64url-encoded to produce
// the "kdi" parameter's value.
const char kKdi[] = "CAESDgoMaHdfcHJvdGVjdGVk";

GURL GetGpmResetPinUrl() {
  std::string command_line_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          webauthn::switches::kGpmPinResetReauthUrlSwitch);
  if (command_line_url.empty()) {
    // Command line switch is not specified or is not a valid ASCII string.
    return GURL(kGpmPinResetReauthUrl);
  }
  return GURL(command_line_url);
}

// This WebContents observer watches the WebView that shows a GAIA
// reauthentication page. When that page navigates to a URL that includes the
// resulting RAPT token, it invokes a callback with that token.
class ReauthWebContentsObserver : public content::WebContentsObserver {
 public:
  ReauthWebContentsObserver(content::WebContents* web_contents,
                            const GURL& reauth_url,
                            base::OnceCallback<void(std::string)> callback)
      : content::WebContentsObserver(web_contents),
        reauth_url_(reauth_url),
        callback_(std::move(callback)) {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!callback_ || !navigation_handle->IsInPrimaryMainFrame() ||
        !url::IsSameOriginWith(reauth_url_, navigation_handle->GetURL()) ||
        !navigation_handle->GetResponseHeaders() ||
        !IsValidHttpStatus(
            navigation_handle->GetResponseHeaders()->response_code())) {
      return;
    }

    std::string rapt;
    if (!net::GetValueForKeyInQuery(navigation_handle->GetURL(), "rapt",
                                    &rapt)) {
      return;
    }

    std::move(callback_).Run(std::move(rapt));
  }

 private:
  static bool IsValidHttpStatus(int status) {
    return status == net::HTTP_OK || status == net::HTTP_NO_CONTENT;
  }

  const GURL reauth_url_;
  base::OnceCallback<void(std::string)> callback_;
};

// Shows a pop-up window containing some WebAuthn-related UI. This object
// owns itself.
class AuthenticatorRequestWindow
    : public content::WebContentsObserver,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit AuthenticatorRequestWindow(content::WebContents* caller_web_contents,
                                      AuthenticatorRequestDialogModel* model)
      : step_(model->step()), model_(model) {
    model_->observers.AddObserver(this);

    // The original profile is used so that cookies are available. If this is an
    // Incognito session then a warning has already been shown to the user.
    Profile* const profile =
        Profile::FromBrowserContext(caller_web_contents->GetBrowserContext())
            ->GetOriginalProfile();
    // If the profile is shutting down, don't attempt to create a pop-up.
    if (Browser::GetCreationStatusForProfile(profile) !=
        Browser::CreationStatus::kOk) {
      return;
    }

    // The pop-up window will be centered on top of the Browser doing the
    // WebAuthn operation.
    Browser* const caller_browser =
        chrome::FindBrowserWithTab(caller_web_contents);
    const gfx::Rect caller_bounds = caller_browser->window()->GetBounds();
    const gfx::Point caller_center = caller_bounds.CenterPoint();

    Browser::CreateParams browser_params(Browser::TYPE_POPUP, profile,
                                         /*user_gesture=*/true);
    browser_params.omit_from_session_restore = true;
    browser_params.should_trigger_session_restore = false;
    // This is empirically a good size for the MagicArch UI.
    constexpr int kWidth = 400;
    constexpr int kHeight = 700;
    browser_params.initial_bounds =
        gfx::Rect(caller_center.x() - kWidth / 2,
                  caller_center.y() - kHeight / 2, kWidth, kHeight);
    browser_params.initial_origin_specified =
        Browser::ValueSpecified::kSpecified;
    auto* browser = Browser::Create(browser_params);

    content::WebContents::CreateParams webcontents_params(profile);
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(webcontents_params);
    WebContentsObserver::Observe(web_contents.get());

    GURL url;
    switch (step_) {
      case AuthenticatorRequestDialogModel::Step::kRecoverSecurityDomain:
        url = GaiaUrls::GetInstance()->gaia_url().Resolve(
            base::StrCat({"/encryption/unlock/desktop?kdi=", kKdi}));
        device::enclave::RecordEvent(device::enclave::Event::kRecoveryShown);
        break;

      case AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset:
        url = GetGpmResetPinUrl();
        reauth_observer_ = std::make_unique<ReauthWebContentsObserver>(
            web_contents.get(), url,
            // Unretained: `reauth_observer_` is owned by this object so if
            // it exists, this object also exists.
            base::BindOnce(&AuthenticatorRequestWindow::OnHaveToken,
                           base::Unretained(this)));
        break;

      default:
        NOTREACHED_NORETURN();
    }

    content::NavigationController::LoadURLParams load_params(url);
    web_contents->GetController().LoadURLWithParams(load_params);
    web_contents_weak_ptr_ = web_contents->GetWeakPtr();

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), /*index=*/0,
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
        AddTabTypes::ADD_ACTIVE);
    browser->window()->Show();
  }

  ~AuthenticatorRequestWindow() override {
    if (model_) {
      model_->observers.RemoveObserver(this);
    }
  }

 protected:
  void WebContentsDestroyed() override {
    WebContentsObserver::Observe(nullptr);
    if (model_ && model_->step() == step_) {
      model_->OnRecoverSecurityDomainClosed();
    }
  }

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    CloseWindowAndDeleteSelf();
  }

  void OnStepTransition() override {
    if (model_->step() != step_) {
      // Only one UI step involves a window so far. So any transition of the
      // model must be to a step that doesn't have one.
      CloseWindowAndDeleteSelf();
    }
  }

 private:
  void CloseWindowAndDeleteSelf() {
    if (web_contents_weak_ptr_) {
      web_contents_weak_ptr_->Close();
    }
    delete this;
  }

  void OnHaveToken(std::string rapt) {
    if (model_) {
      model_->OnReauthComplete(std::move(rapt));
    }
  }

  const AuthenticatorRequestDialogModel::Step step_;
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<ReauthWebContentsObserver> reauth_observer_;
  base::WeakPtr<content::WebContents> web_contents_weak_ptr_;
};

}  // namespace

void ShowAuthenticatorRequestWindow(content::WebContents* web_contents,
                                    AuthenticatorRequestDialogModel* model) {
  // This object owns itself.
  new AuthenticatorRequestWindow(web_contents, model);
}

bool IsAuthenticatorRequestWindowUrl(const GURL& url) {
  std::string kdi;
  return net::GetValueForKeyInQuery(url, "kdi", &kdi) && kdi == kKdi;
}
