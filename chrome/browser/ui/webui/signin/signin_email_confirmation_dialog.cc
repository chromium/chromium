// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_email_confirmation_dialog.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/signin/signin_email_confirmation_ui.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace {

// Dialog size.
const int kSigninEmailConfirmationDialogWidth = 512;
const int kSigninEmailConfirmationDialogMinHeight = 200;
const int kSigninEmailConfirmationDialogMaxHeight = 700;

// Dialog action key;
const char kSigninEmailConfirmationActionKey[] = "action";

// Dialog action values.
const char kSigninEmailConfirmationActionCancel[] = "cancel";
const char kSigninEmailConfirmationActionCreateNewUser[] = "createNewUser";
const char kSigninEmailConfirmationActionStartSync[] = "startSync";

}  // namespace

class SigninEmailConfirmationDialog::DialogWebContentsObserver
    : public content::WebContentsObserver {
 public:
  DialogWebContentsObserver(content::WebContents* web_contents,
                            SigninEmailConfirmationDialog* dialog)
      : content::WebContentsObserver(web_contents),
        signin_email_confirmation_dialog_(dialog) {}

  DialogWebContentsObserver(const DialogWebContentsObserver&) = delete;
  DialogWebContentsObserver& operator=(const DialogWebContentsObserver&) =
      delete;

  ~DialogWebContentsObserver() override {}

 private:
  void WebContentsDestroyed() override {
    // The dialog is already closed. No need to call CloseDialog() again.
    // NOTE: |this| is deleted after |ResetDialogObserver| returns.
    signin_email_confirmation_dialog_->ResetDialogObserver();
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    signin_email_confirmation_dialog_->CloseDialog();
  }

  const raw_ptr<SigninEmailConfirmationDialog>
      signin_email_confirmation_dialog_;
};

SigninEmailConfirmationDialog::SigninEmailConfirmationDialog(
    content::WebContents* contents,
    Profile* profile,
    const std::string& last_email,
    const std::string& new_email,
    Callback callback)
    : web_contents_(contents),
      profile_(profile),
      callback_(std::move(callback)) {
  set_can_close(true);
  set_dialog_modal_type(ui::mojom::ModalType::kWindow);
  set_dialog_content_url(GURL(chrome::kChromeUISigninEmailConfirmationURL));
  // This dialog chooses its height automatically based on its contents.
  set_dialog_size(gfx::Size(kSigninEmailConfirmationDialogWidth, 0));
  set_dialog_args(*base::WriteJson(base::Value::Dict()
                                       .Set("newEmail", new_email)
                                       .Set("lastEmail", last_email)));
  set_show_dialog_title(false);
}

SigninEmailConfirmationDialog::~SigninEmailConfirmationDialog() {}

// static
SigninEmailConfirmationDialog*
SigninEmailConfirmationDialog::AskForConfirmation(
    content::WebContents* contents,
    Profile* profile,
    const std::string& last_email,
    const std::string& email,
    Callback callback) {
  base::RecordAction(base::UserMetricsAction("Signin_Show_ImportDataPrompt"));
  // ShowDialog() will take care of ownership.
  SigninEmailConfirmationDialog* dialog = new SigninEmailConfirmationDialog(
      contents, profile, last_email, email, std::move(callback));
  dialog->ShowDialog();
  return dialog;
}

void SigninEmailConfirmationDialog::ShowDialog() {
  gfx::Size min_size(kSigninEmailConfirmationDialogWidth,
                     kSigninEmailConfirmationDialogMinHeight);
  gfx::Size max_size(kSigninEmailConfirmationDialogWidth,
                     kSigninEmailConfirmationDialogMaxHeight);
  ConstrainedWebDialogDelegate* dialog_delegate =
      ShowConstrainedWebDialogWithAutoResize(profile_, base::WrapUnique(this),
                                             web_contents_, min_size, max_size);

  content::WebContents* dialog_web_contents = dialog_delegate->GetWebContents();

  // Clear the zoom level for the dialog so that it is not affected by the page
  // zoom setting.
  const GURL dialog_url = GetDialogContentURL();
  content::HostZoomMap::Get(dialog_web_contents->GetSiteInstance())
      ->SetZoomLevelForHostAndScheme(dialog_url.scheme(), dialog_url.host(), 0);

  dialog_observer_ =
      std::make_unique<DialogWebContentsObserver>(dialog_web_contents, this);
}

void SigninEmailConfirmationDialog::CloseDialog() {
  content::WebContents* dialog_web_contents = GetDialogWebContents();
  if (!dialog_web_contents)
    return;

  content::WebUI* web_ui = dialog_web_contents->GetWebUI();
  if (web_ui) {
    SigninEmailConfirmationUI* signin_email_confirmation_ui =
        static_cast<SigninEmailConfirmationUI*>(web_ui->GetController());
    if (signin_email_confirmation_ui)
      signin_email_confirmation_ui->Close();
  }
}

void SigninEmailConfirmationDialog::ResetDialogObserver() {
  dialog_observer_.reset();
}

content::WebContents* SigninEmailConfirmationDialog::GetDialogWebContents()
    const {
  return dialog_observer_.get() ? dialog_observer_->web_contents() : nullptr;
}

// ui::WebDialogDelegate implementation

void SigninEmailConfirmationDialog::OnDialogClosed(
    const std::string& json_retval) {
  Action action = CLOSE;
  std::optional<base::Value> ret_value = base::JSONReader::Read(json_retval);
  if (ret_value && ret_value->is_dict()) {
    const std::string* action_string =
        ret_value->GetDict().FindString(kSigninEmailConfirmationActionKey);
    if (action_string) {
      if (*action_string == kSigninEmailConfirmationActionCancel) {
        action = CLOSE;
      } else if (*action_string ==
                 kSigninEmailConfirmationActionCreateNewUser) {
        action = CREATE_NEW_USER;
      } else if (*action_string == kSigninEmailConfirmationActionStartSync) {
        action = START_SYNC;
      } else {
        NOTREACHED_IN_MIGRATION()
            << "Unexpected action value [" << *action_string << "]";
      }
    } else {
      NOTREACHED_IN_MIGRATION()
          << "No action in the dialog close return arguments";
    }
  } else {
    // If the dialog is dismissed without any return value, then simply close
    // the dialog. (see http://crbug.com/667690)
    action = CLOSE;
  }

  NotifyModalDialogClosed();

  if (callback_)
    std::move(callback_).Run(action);
}

void SigninEmailConfirmationDialog::CloseModalSignin() {
  CloseDialog();
}

void SigninEmailConfirmationDialog::ResizeNativeView(int height) {
  NOTIMPLEMENTED();
}

content::WebContents* SigninEmailConfirmationDialog::GetWebContents() {
  return GetDialogWebContents();
}

void SigninEmailConfirmationDialog::SetWebContents(
    content::WebContents* web_contents) {
  NOTIMPLEMENTED();
}
