// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_modal_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

class GURL;

namespace chromeos {

// Extends from |SystemWebDialogDelegate| to create an always-on-top but movable
// dialog. It is intentionally made movable so that users can copy-paste account
// passwords from password managers.
class InlineLoginDialogChromeOS : public SystemWebDialogDelegate,
                                  public web_modal::WebContentsModalDialogHost {
 public:
  static const char kAccountAdditionSource[];

  // The source UX surface used for launching the account addition /
  // re-authentication dialog. This should be as specific as possible.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Note: Please update |AccountManagerAccountAdditionSource| in enums.xml
  // after adding new values.
  enum class Source : int {
    // Settings > Add account button.
    kSettingsAddAccountButton = 0,
    // Settings > Sign in again button.
    kSettingsReauthAccountButton = 1,
    // Launched from an ARC application.
    kArc = 2,
    // Launched automatically from Chrome content area. As of now, this is
    // possible only when an account requires re-authentication.
    kContentArea = 3,
    // Print Preview dialog.
    kPrintPreviewDialog = 4,
    // Account Manager migration welcome screen.
    kAccountManagerMigrationWelcomeScreen = 5,
    // Onboarding.
    kOnboarding = 6,

    kMaxValue = kOnboarding
  };

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

  // Displays the dialog. |email| pre-fills the account email field in the
  // sign-in dialog - useful for account re-authentication. |source| specifies
  // the source UX surface used for launching the dialog.
  static void Show(const std::string& email, const Source& source);

  // Displays the dialog for account addition. |source| specifies the source UX
  // surface used for launching the dialog.
  static void Show(const Source& source);

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
  explicit InlineLoginDialogChromeOS(const Source& source);
  InlineLoginDialogChromeOS(const GURL& url, const Source& source);
  ~InlineLoginDialogChromeOS() override;

  // ui::WebDialogDelegate overrides
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  bool ShouldShowDialogTitle() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

 private:
  InlineLoginHandlerModalDelegate delegate_;
  const Source source_;
  const GURL url_;
  base::Optional<EduCoexistenceFlowResult> edu_coexistence_flow_result_;

  DISALLOW_COPY_AND_ASSIGN(InlineLoginDialogChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_H_
