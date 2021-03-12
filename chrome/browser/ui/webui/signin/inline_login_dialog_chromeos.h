// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_H_

#include <string>

#include "ash/components/account_manager/account_manager.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_modal_delegate.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

class GURL;

namespace ash {
class AccountManagerUIImpl;
}

namespace chromeos {

// Extends from |SystemWebDialogDelegate| to create an always-on-top but movable
// dialog. It is intentionally made movable so that users can copy-paste account
// passwords from password managers.
class InlineLoginDialogChromeOS : public SystemWebDialogDelegate,
                                  public web_modal::WebContentsModalDialogHost {
 public:
  // Represents the last reached step in the flow.
  // Keep in sync with
  // chrome/browser/resources/chromeos/edu_login/edu_login_util.js
  // Used in UMA, do not delete or reorder values.
  // Note: Please update enums.xml after adding new values.
  enum class EduCoexistenceFlowResult : int {
    kParentsListScreen = 0,
    kParentPasswordScreen = 1,
    kParentInfoScreen1 = 2,
    kParentInfoScreen2 = 3,
    kEduAccountLoginScreen = 4,
    kFlowCompleted = 5,
    kMaxValue = kFlowCompleted
  };

  static bool IsShown();

  // Displays the dialog. |email| pre-fills the account email field in the
  // sign-in dialog - useful for account re-authentication. |source| specifies
  // the source UX surface used for launching the dialog.
  // DEPRECATED: Use AccountManagerFacade instead (see
  // https://crbug.com/1140469).
  static void ShowDeprecated(
      const std::string& email,
      const ::account_manager::AccountManagerFacade::AccountAdditionSource&
          source);

  // Displays the dialog for account addition. |source| specifies the source UX
  // surface used for launching the dialog.
  // DEPRECATED: Use AccountManagerFacade instead (see
  // https://crbug.com/1140469).
  static void ShowDeprecated(
      const ::account_manager::AccountManagerFacade::AccountAdditionSource&
          source);

  // Updates the value of the last reached step in 'Add Account' flow for child
  // users. Before the dialog will close, this value will be reported to UMA.
  static void UpdateEduCoexistenceFlowResult(EduCoexistenceFlowResult result);

  // ui::SystemWebDialogDelegate overrides.
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

  // web_modal::WebContentsModalDialogHost overrides.
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

  void SetEduCoexistenceFlowResult(EduCoexistenceFlowResult result);

 protected:
  InlineLoginDialogChromeOS();
  explicit InlineLoginDialogChromeOS(const GURL& url);

  InlineLoginDialogChromeOS(const GURL& url,
                            base::OnceClosure close_dialog_closure);
  ~InlineLoginDialogChromeOS() override;

  // ui::WebDialogDelegate overrides
  void GetDialogSize(gfx::Size* size) const override;
  ui::ModalType GetDialogModalType() const override;
  bool ShouldShowDialogTitle() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

 private:
  // `Show` method can be called directly only by `AccountManagerUIImpl` class.
  // To show the dialog, use `AccountManagerFacade`.
  friend class ash::AccountManagerUIImpl;

  // Displays the dialog. |close_dialog_closure| will be called when the dialog
  // is closed.
  static void Show(base::OnceClosure close_dialog_closure);

  // Displays the dialog. |email| pre-fills the account email field in the
  // sign-in dialog - useful for account re-authentication.
  // |close_dialog_closure| will be called when the dialog is closed.
  static void Show(const std::string& email,
                   base::OnceClosure close_dialog_closure);

  static void ShowInternal(
      const std::string& email,
      base::OnceClosure close_dialog_closure = base::DoNothing());

  InlineLoginHandlerModalDelegate delegate_;
  const GURL url_;
  base::Optional<EduCoexistenceFlowResult> edu_coexistence_flow_result_;
  base::OnceClosure close_dialog_closure_;
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginDialogChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_H_
