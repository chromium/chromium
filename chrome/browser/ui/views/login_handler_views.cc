// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/login_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chrome {

namespace {

// This class simply forwards the authentication from the LoginView (on
// the UI thread) to the net::URLRequest (on the I/O thread).
// This class uses ref counting to ensure that it lives until all InvokeLaters
// have been called.
class LoginHandlerViews : public LoginHandler {
 public:
  LoginHandlerViews(const net::AuthChallengeInfo& auth_info,
                    content::WebContents* web_contents,
                    LoginAuthRequiredCallback auth_required_callback)
      : LoginHandler(auth_info,
                     web_contents,
                     std::move(auth_required_callback)),
        popunder_preventer_(std::make_unique<PopunderPreventer>(web_contents)) {
    RecordDialogCreation(DialogIdentifier::LOGIN_HANDLER);
  }

  ~LoginHandlerViews() override {
    // LoginHandler cannot call CloseDialog because the subclass will already
    // have been destructed.
    CloseDialog();
  }

 protected:
  // LoginHandler:
  void BuildViewImpl(const base::string16& authority,
                     const base::string16& explanation,
                     LoginModelData* login_model_data) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!dialog_);

    dialog_ = new Dialog(this, web_contents(), authority, explanation,
                         login_model_data);
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
           const base::string16& authority,
           const base::string16& explanation,
           LoginHandler::LoginModelData* login_model_data)
        : handler_(handler), login_view_(nullptr), widget_(nullptr) {
      DialogDelegate::set_button_label(
          ui::DIALOG_BUTTON_OK,
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL));

      // Create a new LoginView and set the model for it.  The model (password
      // manager) is owned by the WebContents, but the view is parented to the
      // browser window, so the view may be destroyed after the password
      // manager. The view listens for model destruction and unobserves
      // accordingly.
      login_view_ = new LoginView(authority, explanation, login_model_data);

      // ShowWebModalDialogViews takes ownership of this, by way of the
      // DeleteDelegate method.
      widget_ = constrained_window::ShowWebModalDialogViews(this, web_contents);
    }

    void CloseDialog() {
      handler_ = nullptr;
      // The hosting widget may have been freed.
      if (widget_)
        widget_->Close();
    }

    // views::DialogDelegate:
    bool ShouldShowCloseButton() const override { return false; }

    base::string16 GetWindowTitle() const override {
      return l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE);
    }

    void WindowClosing() override {
      // Reference is no longer valid.
      widget_ = nullptr;
      if (handler_)
        handler_->CancelAuth();
    }

    void DeleteDelegate() override { delete this; }

    ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

    bool Cancel() override {
      DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
      if (handler_)
        handler_->CancelAuth();
      return true;
    }

    bool Accept() override {
      DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
      if (handler_)
        handler_->SetAuth(login_view_->GetUsername(),
                          login_view_->GetPassword());
      return true;
    }

    views::View* GetInitiallyFocusedView() override {
      return login_view_->GetInitiallyFocusedView();
    }

    views::View* GetContentsView() override { return login_view_; }
    views::Widget* GetWidget() override { return login_view_->GetWidget(); }
    const views::Widget* GetWidget() const override {
      return login_view_->GetWidget();
    }

   private:
    ~Dialog() override {
      if (handler_)
        handler_->OnDialogDestroyed();
    }

    LoginHandlerViews* handler_;
    // The LoginView that contains the user's login information.
    LoginView* login_view_;
    views::Widget* widget_;

    DISALLOW_COPY_AND_ASSIGN(Dialog);
  };

  Dialog* dialog_ = nullptr;
  std::unique_ptr<PopunderPreventer> popunder_preventer_;

  DISALLOW_COPY_AND_ASSIGN(LoginHandlerViews);
};

}  // namespace

std::unique_ptr<LoginHandler> CreateLoginHandlerViews(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<LoginHandlerViews>(auth_info, web_contents,
                                             std::move(auth_required_callback));
}

}  // namespace chrome
