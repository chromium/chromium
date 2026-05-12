// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/authenticator_request_window.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker_impl.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/fido/enclave/metrics.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/origin.h"

namespace {

constexpr std::string_view kMagicArchUrl = "https://passwords.google.com";
constexpr std::string_view kGpmPasskeyPinResetPath = "/encryption/pin/reset";
constexpr std::string_view kGpmPasskeyResetSuccessPath =
    "/embedded/passkeys/reset/done";
constexpr std::string_view kGpmPasskeyResetFailPath =
    "/embedded/passkeys/reset/error";

GURL GetGpmMagicArchUrl() {
  std::string command_line_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          webauthn::switches::kGpmMagicArchUrlSwitch);
  if (command_line_url.empty()) {
    // Command line switch is not specified or is not a valid ASCII string.
    return GURL(kMagicArchUrl);
  }
  return GURL(command_line_url);
}

// This WebContents observer watches the WebView that shows a GAIA
// reauthentication page. When that page navigates to a URL that includes the
// resulting RAPT token, it invokes a callback with that token.
class ReauthWebContentsObserver : public content::WebContentsObserver {
 public:
  ReauthWebContentsObserver(content::WebContents* web_contents,
                            base::OnceCallback<void(std::string)> callback)
      : content::WebContentsObserver(web_contents),
        callback_(std::move(callback)) {}

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    const GURL& url = navigation_handle->GetURL();
    if (!callback_ || !navigation_handle->IsInPrimaryMainFrame() ||
        !url::IsSameOriginWith(GetGpmMagicArchUrl(), url) ||
        !navigation_handle->GetResponseHeaders() ||
        !IsValidHttpStatus(
            navigation_handle->GetResponseHeaders()->response_code())) {
      return;
    }

    std::string rapt;
    if (!net::GetValueForKeyInQuery(url, "rapt", &rapt)) {
      return;
    }

    // Post a task to avoid attempting to close the web contents during
    // execution of an observer method, which can trigger a crash.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(rapt)));
  }

 private:
  static bool IsValidHttpStatus(int status) {
    return status == net::HTTP_OK || status == net::HTTP_NO_CONTENT;
  }

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
        !url::IsSameOriginWith(url, GetGpmMagicArchUrl()) || !url.has_path()) {
      return;
    }
    status_ = Status::kStarted;
    if (url.GetPath() == kGpmPasskeyResetSuccessPath) {
      status_ = Status::kSuccess;
    } else if (url.GetPath() == kGpmPasskeyResetFailPath) {
      status_ = Status::kFail;
    }
    MaybeRunCallback(url.has_ref() ? url.GetRef() : "");
  }

  Status status() const { return status_; }

 private:
  void MaybeRunCallback(const std::string& ref) {
    if (status_ == Status::kStarted || ref.empty()) {
      return;
    }
    // Reset `status_` to avoid automatically closing the web contents resulting
    // in calling `OnPasskeysReset` again.
    Status status = status_;
    status_ = Status::kNotStarted;
    if (status == Status::kSuccess && ref == "success") {
      // Post a task to avoid attempting to close the web contents during
      // execution of an observer method, which can trigger a crash.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), true));
    }
    if (status == Status::kFail && ref == "fail") {
      // Post a task to avoid attempting to close the web contents during
      // execution of an observer method, which can trigger a crash.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), false));
    }
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
    model_observation_.Observe(model_);

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
    BrowserWindowInterface* const caller_browser =
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            caller_web_contents);
    const gfx::Rect caller_bounds = caller_browser->GetWindow()->GetBounds();
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
      case AuthenticatorRequestDialogModel::Step::kGPMRecoverSecurityDomain: {
        signin::IdentityManager* identity_manager =
            IdentityManagerFactory::GetForProfile(profile);
        // Default to the first account if the account is not present in the
        // cookie jar. This can happen in tests, or if account state changed
        // right before this code.
        size_t account_index =
            identity_manager->GetSessionIndexForPrimaryAccount().value_or(0u);
        url = GaiaUrls::GetInstance()->SigninChromePasskeyUnlockUrl(
            account_index);
        device::enclave::RecordEvent(device::enclave::Event::kRecoveryShown);
        webauthn::user_actions::RecordRecoveryShown(model_->request_type);
        passkey_reset_observer_ =
            std::make_unique<PasskeyResetWebContentsObserver>(
                web_contents.get(),
                base::BindOnce(&AuthenticatorRequestWindow::OnPasskeysReset,
                               weak_ptr_factory_.GetWeakPtr()));
        break;
      }
      case AuthenticatorRequestDialogModel::Step::kGPMReauthForPinReset:
        url = GetGpmMagicArchUrl().Resolve(kGpmPasskeyPinResetPath);
        reauth_observer_ = std::make_unique<ReauthWebContentsObserver>(
            web_contents.get(),
            base::BindOnce(&AuthenticatorRequestWindow::OnHaveToken,
                           weak_ptr_factory_.GetWeakPtr()));
        break;

      default:
        NOTREACHED();
    }

    content::NavigationController::LoadURLParams load_params(url);
    base::WeakPtr<content::NavigationHandle> navigation_handle =
        web_contents->GetController().LoadURLWithParams(load_params);
    web_contents_weak_ptr_ = web_contents->GetWeakPtr();

    if (navigation_handle &&
        step_ ==
            AuthenticatorRequestDialogModel::Step::kGPMRecoverSecurityDomain) {
      TrustedVaultEncryptionKeysTabHelper* encryption_keys_tab_helper =
          TrustedVaultEncryptionKeysTabHelper::FromWebContents(
              navigation_handle->GetWebContents());
      if (encryption_keys_tab_helper) {
        encryption_keys_tab_helper->SetUserActionTrigger(
            trusted_vault::TrustedVaultUserActionTriggerForUMA::
                kPasskeyBootstrappingFlow);
      }
    }

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), /*index=*/0,
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
        AddTabTypes::ADD_ACTIVE);
    browser->window()->Show();
  }

  ~AuthenticatorRequestWindow() override = default;

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
      model_->OnGPMRecoverSecurityDomainClosed();
    }
  }

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    CloseWindowAndDeleteSelf();
  }

  void OnStepTransition() override {
    if (model_->step() != step_) {
      // No UI step involving a window leads to another step involving another
      // window. So any transition of the model must be to a step that doesn't
      // have one.
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
      model_->OnGPMReauthComplete(std::move(rapt));
    }
  }

  void OnPasskeysReset(bool success) {
    if (model_) {
      model_->OnGPMPasskeysReset(success);
    }
  }

  const AuthenticatorRequestDialogModel::Step step_;
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  std::unique_ptr<ReauthWebContentsObserver> reauth_observer_;
  std::unique_ptr<PasskeyResetWebContentsObserver> passkey_reset_observer_;
  base::WeakPtr<content::WebContents> web_contents_weak_ptr_;
  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestWindow>
      model_observation_{this};
  base::WeakPtrFactory<AuthenticatorRequestWindow> weak_ptr_factory_{this};
};

}  // namespace

void ShowAuthenticatorRequestWindow(content::WebContents* web_contents,
                                    AuthenticatorRequestDialogModel* model) {
  // This object owns itself.
  new AuthenticatorRequestWindow(web_contents, model);
}

bool IsAuthenticatorRequestWindowUrl(const GURL& url) {
  std::string kdi;
  return net::GetValueForKeyInQuery(url, "kdi", &kdi) &&
         kdi == GaiaUrls::GetInstance()
                    ->signin_chrome_passkey_unlock_kdi_parameter();
}
