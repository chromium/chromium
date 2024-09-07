// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/authenticator_request_window.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/fido/enclave/metrics.h"
#include "device/fido/features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

namespace {

const char kGpmPinResetReauthUrl[] =
    "https://passwords.google.com/encryption/pin/reset";
const char kGpmPasskeyResetSuccessUrl[] =
    "https://passwords.google.com/embedded/passkeys/reset/done";
const char kGpmPasskeyResetFailUrl[] =
    "https://passwords.google.com/embedded/passkeys/reset/error";

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
    const GURL& url = navigation_handle->GetURL();
    if (!callback_ || !navigation_handle->IsInPrimaryMainFrame() ||
        !url::IsSameOriginWith(reauth_url_, url) ||
        !navigation_handle->GetResponseHeaders() ||
        !IsValidHttpStatus(
            navigation_handle->GetResponseHeaders()->response_code())) {
      return;
    }

    std::string rapt;
    if (!net::GetValueForKeyInQuery(url, "rapt", &rapt)) {
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

// The user may be prompted to reset their passkeys if the MagicArch PIN
// challenge fails. MagicArch will then navigate to either
// `kGpmPasskeyResetSuccessUrl` or `kGpmPasskeyResetFailUrl` after completing a
// reset. The pages have a message on and a button. If the user clicks the
// button, a ref is appended to the URL. This observer will observe which page
// the user is on and call the `callback_`.
class PasskeyResetWebContentsObserver : public content::WebContentsObserver {
 public:
  enum class Status {
    // Passkeys reset flow not started. The user is still in the PIN screen.
    kNotStarted,
    // Passkeys reset flow not completed.
    kStarted,
    // Passkeys reset flow succeeded. The user has not clicked the button in the
    // page yet.
    kSuccess,
    // Passkeys reset flow failed. The user has not clicked the button in the
    // page yet.
    kFail,
  };
  PasskeyResetWebContentsObserver(content::WebContents* web_contents,
                                  base::OnceCallback<void(bool)> callback)
      : content::WebContentsObserver(web_contents),
        callback_(std::move(callback)) {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    const GURL& url = navigation_handle->GetURL();
    if (!callback_ || !navigation_handle->IsInPrimaryMainFrame() ||
        !url::IsSameOriginWith(url, GURL(kGpmPasskeyResetSuccessUrl)) ||
        !url.has_path()) {
      return;
    }
    status_ = Status::kStarted;
    if (url.path() == GURL(kGpmPasskeyResetSuccessUrl).path()) {
      status_ = Status::kSuccess;
    } else if (url.path() == GURL(kGpmPasskeyResetFailUrl).path()) {
      status_ = Status::kFail;
    }

    MaybeRunCallback(url.has_ref() ? url.ref() : "");
  }

  Status status() const { return status_; }

 private:
  void MaybeRunCallback(const std::string& ref) {
    if (status_ == Status::kStarted || ref.empty()) {
      return;
    }
    if (status_ == Status::kSuccess && ref == "success") {
      std::move(callback_).Run(true);
    } else if (status_ == Status::kFail && ref == "fail") {
      std::move(callback_).Run(false);
    }
    status_ = Status::kNotStarted;
  }

  Status status_ = Status::kNotStarted;
  base::OnceCallback<void(bool)> callback_;
};

// Shows a pop-up window containing some WebAuthn-related UI. This object
// owns itself.
class AuthenticatorRequestWindow
    : public content::WebContentsObserver,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  using PasskeyResetStatus = PasskeyResetWebContentsObserver::Status;
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
    // This is empirically a good size for the MagicArch UI. (Note that the UI
    // is much larger when the user needs to enter an unlock pattern, so don't
    // size this purely based on PIN entry.)
    constexpr int kWidth = 900;
    constexpr int kHeight = 750;
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
        webauthn::user_actions::RecordRecoveryShown(
            /*is_create=*/model_->request_type ==
            device::FidoRequestType::kMakeCredential);
        if (base::FeatureList::IsEnabled(device::kWebAuthnPasskeysReset)) {
          passkey_reset_observer_ =
              std::make_unique<PasskeyResetWebContentsObserver>(
                  web_contents.get(),
                  // Unretained: `passkey_reset_observer_` is owned by this
                  // object so if it exists, this object also exists.
                  base::BindOnce(&AuthenticatorRequestWindow::OnPasskeysReset,
                                 base::Unretained(this)));
        }
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
        NOTREACHED();
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
    if (!model_) {
      return;
    }
    // If the passkey reset is complete and the user has not clicked the button
    // in the page, we still wish to react to the reset.
    if (passkey_reset_observer_ &&
        (passkey_reset_observer_->status() == PasskeyResetStatus::kSuccess ||
         passkey_reset_observer_->status() == PasskeyResetStatus::kFail)) {
      OnPasskeysReset(passkey_reset_observer_->status() ==
                      PasskeyResetStatus::kSuccess);
      return;
    }
    if (model_->step() == step_) {
      webauthn::user_actions::RecordRecoveryCancelled();
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

  void OnPasskeysReset(bool success) {
    if (model_) {
      model_->OnGpmPasskeysReset(success);
    }
  }

  const AuthenticatorRequestDialogModel::Step step_;
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<ReauthWebContentsObserver> reauth_observer_;
  std::unique_ptr<PasskeyResetWebContentsObserver> passkey_reset_observer_;
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
