// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/views/login_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// This class prompts the user for credentials and returns the result to
// `auth_required_callback`.
class LoginHandlerViews : public LoginHandler {
 public:
  LoginHandlerViews(const net::AuthChallengeInfo& auth_info,
                    content::WebContents* web_contents,
                    LoginAuthRequiredCallback auth_required_callback)
      : LoginHandler(auth_info,
                     web_contents,
                     std::move(auth_required_callback)),
        popunder_preventer_(std::make_unique<PopunderPreventer>(web_contents)) {
  }

  LoginHandlerViews(const LoginHandlerViews&) = delete;
  LoginHandlerViews& operator=(const LoginHandlerViews&) = delete;

  ~LoginHandlerViews() override {
    // LoginHandler cannot call CloseDialog because the subclass will already
    // have been destructed.
    CloseDialog();
  }

 protected:
  // LoginHandler:
  bool BuildViewImpl(const std::u16string& authority,
                     const std::u16string& explanation,
                     LoginModelData* login_model_data) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!dialog_);

    // A WebContentsModalDialogManager is necessary to show the Dialog. A
    // manager may not be available during the shutdown process of the
    // WebContents, which can trigger DidFinishNavigation events.
    // See https://crbug.com/328462789.
    web_modal::WebContentsModalDialogManager* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(
            constrained_window::GetTopLevelWebContents(web_contents()));
    if (!manager || !manager->delegate()) {
      return false;
    }
    dialog_ = new Dialog(this, web_contents(), authority, explanation,
                         login_model_data);
    return true;
  }

  void CloseDialog() override {
    // The hosting widget may have been freed.
    if (dialog_) {
      dialog_->CloseDialog();
      dialog_ = nullptr;
    }
    popunder_preventer_ = nullptr;
  }

 private:
  void OnDialogDestroyed() { dialog_ = nullptr; }

  // The DialogDelegate is a separate object from LoginHandlerViews so it can be
  // owned by the views hierarchy (see DeleteDelegate).
  class Dialog : public views::DialogDelegate {
   public:
    // Creates a Dialog which reports the results back to |handler|. Note the
    // Dialog is responsible for its own lifetime, which may be independent of
    // |handler|. |handler| may decide to close the Dialog, by calling
    // CloseDialog, or the Dialog may have been destroyed by the views
    // hierarchy, in which case it will call handler->OnDialogDestroyed. When
    // one of these methods is called, whichever comes first, each object must
    // release pointers to the other.
    Dialog(LoginHandlerViews* handler,
           content::WebContents* web_contents,
           const std::u16string& authority,
           const std::u16string& explanation,
           LoginHandler::LoginModelData* login_model_data)
        : handler_(handler), login_view_(nullptr), widget_(nullptr) {
      SetButtonLabel(
          ui::mojom::DialogButton::kOk,
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL));
      SetAcceptCallback(base::BindOnce(
          [](Dialog* dialog) {
            if (!dialog->handler_)
              return;
            dialog->handler_->SetAuth(dialog->login_view_->GetUsername(),
                                      dialog->login_view_->GetPassword());
          },
          base::Unretained(this)));
      SetCancelCallback(base::BindOnce(
          [](Dialog* dialog) {
            if (!dialog->handler_)
              return;
            dialog->handler_->CancelAuth(/*notify_others=*/true);
          },
          base::Unretained(this)));
      SetModalType(ui::mojom::ModalType::kChild);
      SetOwnedByWidget(true);

      // Create a new LoginView and set the model for it.  The model (password
      // manager) is owned by the WebContents, but the view is parented to the
      // browser window, so the view may be destroyed after the password
      // manager. The view listens for model destruction and unobserves
      // accordingly.
      login_view_ = new LoginView(authority, explanation, login_model_data);
      widget_ = constrained_window::ShowWebModalDialogViews(this, web_contents);
    }

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;

    void CloseDialog() {
      handler_ = nullptr;
      // The hosting widget may have been freed.
      if (widget_)
        widget_->Close();
    }

    // views::DialogDelegate:
    bool ShouldShowCloseButton() const override { return false; }

    std::u16string GetWindowTitle() const override {
      return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE);
    }

    void WindowClosing() override {
      // Reference is no longer valid.
      widget_ = nullptr;
      if (handler_)
        handler_->CancelAuth(/*notify_others=*/true);
    }

    views::View* GetInitiallyFocusedView() override {
      return login_view_->GetInitiallyFocusedView();
    }

    views::View* GetContentsView() override { return login_view_; }
    views::Widget* GetWidget() override {
      return login_view_ ? login_view_->GetWidget() : nullptr;
    }
    const views::Widget* GetWidget() const override {
      return login_view_ ? login_view_->GetWidget() : nullptr;
    }

   private:
    ~Dialog() override {
      if (handler_)
        handler_->OnDialogDestroyed();
    }

    raw_ptr<LoginHandlerViews> handler_;
    // The LoginView that contains the user's login information.
    raw_ptr<LoginView> login_view_;
    raw_ptr<views::Widget> widget_;
  };

  raw_ptr<Dialog> dialog_ = nullptr;
  std::unique_ptr<PopunderPreventer> popunder_preventer_;
};

}  // namespace

// static
std::unique_ptr<LoginHandler> LoginHandler::Create(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<LoginHandlerViews>(auth_info, web_contents,
                                             std::move(auth_required_callback));
}
