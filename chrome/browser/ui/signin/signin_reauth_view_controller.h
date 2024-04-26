// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_SIGNIN_REAUTH_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_SIGNIN_SIGNIN_REAUTH_VIEW_CONTROLLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/signin/signin_modal_dialog.h"
#include "chrome/browser/ui/signin/signin_view_controller_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "google_apis/gaia/core_account_id.h"

class Browser;

namespace {
class ReauthWebContentsObserver;
}

namespace content {
class WebContents;
}

namespace signin {
enum class ReauthResult;
class ReauthTabHelper;
}  // namespace signin

// A controller class for the Reauth UI flow.
//
// The reauth flow consists of:
// - Reauth confirmation webUI page. Displayed in a tab-modal dialog.
// - Gaia Reauth page. Loaded from the web. Displayed either in a tab-modal
// dialog or in a new tab if an account requires SAML authentication. May be
// approved automatically. In that case, no UI is displayed to the user.
//
// The Gaia reauth page is loaded in background and gets shown to the user only
// after the user confirms the reauth confirmation dialog.
// TODO(crbug.com/40209493): rename to SigninReauthDialog.
class SigninReauthViewController
    : public SigninModalDialog,
      public SigninViewControllerDelegate::Observer {
 public:
  enum class GaiaReauthType;

  class Observer : public base::CheckedObserver {
   public:
    // Called when the controller gets destroyed. The subclass must stop
    // observing the controller when this is called.
    virtual void OnReauthControllerDestroyed() {}
    // Called when |reauth_type| is determined. Usually it happens when the
    // Gaia Reauth page navigates.
    // |reauth_type| cannot be |GaiaReauthType::kUnknown|.
    virtual void OnGaiaReauthTypeDetermined(GaiaReauthType reauth_type) {}
    // Called when the WebContents displaying the reauth confirmation UI has
    // been swapped with Gaia reauth WebContents.
    virtual void OnGaiaReauthPageShown() {}
  };

  enum class GaiaReauthPageState {
    // The Gaia Reauth page is loading in background.
    kStarted = 0,
    // The first navigation has been committed in background.
    kNavigated = 1,
    // The reauth has been completed and the result is available.
    kDone = 2
  };

  enum class GaiaReauthType {
    kUnknown = 0,
    kAutoApproved = 1,
    kEmbeddedFlow = 2,
    kSAMLFlow = 3
  };

  enum class UIState {
    // Nothing is being displayed.
    kNone = 0,
    // The Reauth confirmation webUI page is being displayed in a modal dialog.
    kConfirmationDialog = 1,
    // The Gaia Reauth page is being displayed in a modal dialog.
    kGaiaReauthDialog = 2,
    // The Gaia Reauth page is being displayed in a tab.
    kGaiaReauthTab = 3
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UserAction {
    // The user clicked on the confirm button in the Reauth confirmation dialog.
    // The Gaia Reauth was auto-approved and did not show up as a next step.
    kClickConfirmButton = 0,
    // The user clicked on the next button in the Reauth confirmation dialog.
    // The Gaia Reauth showed up as a next step.
    kClickNextButton = 1,
    // The user clicked on the cancel button in the Reauth confirmation dialog.
    kClickCancelButton = 2,
    // The user closed the Reauth confirmation dialog without clicking on the
    // cancel button.
    kCloseConfirmationDialog = 3,
    // The user closed the Gaia Reauth page displayed in a dialog.
    kCloseGaiaReauthDialog = 4,
    // The user closed the Gaia Reauth page displayed in a tab.
    kCloseGaiaReauthTab = 5,
    // The user successfully authenticated on the Gaia Reauth page.
    kPassGaiaReauth = 6,
    kMaxValue = kPassGaiaReauth
  };

  SigninReauthViewController(
      Browser* browser,
      const CoreAccountId& account_id,
      signin_metrics::ReauthAccessPoint access_point,
      base::OnceClosure on_close_callback,
      base::OnceCallback<void(signin::ReauthResult)> reauth_callback);

  SigninReauthViewController(const SigninReauthViewController&) = delete;
  SigninReauthViewController& operator=(const SigninReauthViewController&) =
      delete;

  ~SigninReauthViewController() override;

  // SigninModalDialog:
  void CloseModalDialog() override;
  void ResizeNativeView(int height) override;
  content::WebContents* GetModalDialogWebContentsForTesting() override;

  // SigninViewControllerDelegate::Observer:
  void OnModalDialogClosed() override;

  // Called when the user clicks the confirm button in the reauth confirmation
  // dialog.
  // This happens before the Gaia reauth page is shown.
  void OnReauthConfirmed(
      sync_pb::UserConsentTypes::AccountPasswordsConsent consent);
  // Called when the user clicks the cancel button in the reauth confirmation
  // dialog.
  // This happens before the Gaia reauth page is shown.
  void OnReauthDismissed();

  // Called when the Gaia reauth page has navigated.
  void OnGaiaReauthPageNavigated();
  // Called when the Gaia reauth has been completed and the result is available.
  void OnGaiaReauthPageComplete(signin::ReauthResult result);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  GaiaReauthType gaia_reauth_type() { return gaia_reauth_type_; }

 private:
  // Calls |reauth_callback_| with |result| and closes all Reauth UIs.
  void CompleteReauth(signin::ReauthResult result);

  // Notifies about a change in the reauth flow state. Must be called whenever
  // |user_confirmed_reauth_| or |gaia_reauth_page_state_| has changed.
  void OnStateChanged();

  void OnGaiaReauthTypeDetermined(GaiaReauthType reauth_type);

  void RecordClickOnce(UserAction click_action);

  signin::ReauthTabHelper* GetReauthTabHelper();

  void ShowReauthConfirmationDialog();
  void ShowGaiaReauthPage();
  void ShowGaiaReauthPageInDialog();
  void ShowGaiaReauthPageInNewTab();

  // Controller inputs.
  const raw_ptr<Browser> browser_;
  const CoreAccountId account_id_;
  const signin_metrics::ReauthAccessPoint access_point_;
  base::OnceCallback<void(signin::ReauthResult)> reauth_callback_;

  GaiaReauthType gaia_reauth_type_ = GaiaReauthType::kUnknown;

  // Dialog state useful for recording metrics.
  UIState ui_state_ = UIState::kNone;
  bool has_recorded_click_ = false;

  // Delegate displaying the dialog.
  raw_ptr<SigninViewControllerDelegate> dialog_delegate_ = nullptr;
  base::ScopedObservation<SigninViewControllerDelegate,
                          SigninViewControllerDelegate::Observer>
      dialog_delegate_observation_{this};

  // WebContents of the Gaia reauth page.
  std::unique_ptr<content::WebContents> reauth_web_contents_;
  std::unique_ptr<ReauthWebContentsObserver> reauth_web_contents_observer_;
  // Raw pointer is only set if |reauth_web_contents_| was transferred to a new
  // tab for the SAML flow.
  raw_ptr<content::WebContents> raw_reauth_web_contents_ = nullptr;

  // The state of the reauth flow.
  bool user_confirmed_reauth_ = false;
  std::optional<sync_pb::UserConsentTypes::AccountPasswordsConsent> consent_;
  GaiaReauthPageState gaia_reauth_page_state_ = GaiaReauthPageState::kStarted;
  std::optional<signin::ReauthResult> gaia_reauth_page_result_;

  base::ObserverList<Observer, true> observer_list_;

  base::WeakPtrFactory<SigninReauthViewController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SIGNIN_SIGNIN_REAUTH_VIEW_CONTROLLER_H_
