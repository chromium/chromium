// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_handler_modal_delegate.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

class GURL;

namespace ash {

class AccountManagerUIImpl;

// Extends from |SystemWebDialogDelegate| to create an always-on-top dialog.
class InlineLoginDialog : public SystemWebDialogDelegate,
                          public web_modal::WebContentsModalDialogHost {
 public:
  InlineLoginDialog(const InlineLoginDialog&) = delete;
  InlineLoginDialog& operator=(const InlineLoginDialog&) = delete;

  static bool IsShown();

  // ui::SystemWebDialogDelegate overrides.
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

  // web_modal::WebContentsModalDialogHost overrides.
  gfx::Size GetMaximumDialogSize() override;
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 protected:
  FRIEND_TEST_ALL_PREFIXES(InlineLoginDialogTest, ReturnsEmptyDialogArgs);
  FRIEND_TEST_ALL_PREFIXES(InlineLoginDialogTest, ReturnsCorrectDialogArgs);

  InlineLoginDialog();
  explicit InlineLoginDialog(const GURL& url);

  InlineLoginDialog(
      const GURL& url,
      std::optional<account_manager::AccountAdditionOptions> options,
      base::OnceClosure close_dialog_closure);
  ~InlineLoginDialog() override;

  // ui::WebDialogDelegate overrides
  void GetDialogSize(gfx::Size* size) const override;
  ui::mojom::ModalType GetDialogModalType() const override;
  bool ShouldShowDialogTitle() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;
  std::string GetDialogArgs() const override;

 private:
  class ModalDialogManagerCleanup;

  // `Show` method can be called directly only by `AccountManagerUIImpl` class.
  // To show the dialog, use `AccountManagerFacade`.
  friend class AccountManagerUIImpl;

  // Displays the dialog. |close_dialog_closure| will be called when the dialog
  // is closed.
  static void Show(const account_manager::AccountAdditionOptions& options,
                   base::OnceClosure close_dialog_closure);

  // Displays the dialog. |email| pre-fills the account email field in the
  // sign-in dialog - useful for account re-authentication.
  // |close_dialog_closure| will be called when the dialog is closed.
  static void Show(const std::string& email,
                   base::OnceClosure close_dialog_closure);

  static void ShowInternal(
      const std::string& email,
      std::optional<account_manager::AccountAdditionOptions> options,
      base::OnceClosure close_dialog_closure = base::DoNothing());

  std::unique_ptr<ModalDialogManagerCleanup> modal_dialog_manager_cleanup_;
  InlineLoginHandlerModalDelegate delegate_;
  const GURL url_;
  std::optional<account_manager::AccountAdditionOptions> add_account_options_;
  base::OnceClosure close_dialog_closure_;
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_host_observer_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_DIALOG_H_
