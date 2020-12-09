// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/user_manager_profile_dialog_delegate.h"

#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/views/profiles/user_manager_profile_dialog_host.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

UserManagerProfileDialogDelegate::UserManagerProfileDialogDelegate(
    UserManagerProfileDialogHost* host,
    std::unique_ptr<views::WebView> web_view,
    const GURL& url)
    : host_(host) {
  SetHasWindowSizeControls(true);
  SetTitle(IDS_PROFILES_GAIA_SIGNIN_TITLE);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_use_custom_frame(false);

  web_view_ = AddChildView(std::move(web_view));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  web_view_->GetWebContents()->SetDelegate(this);

  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_view_->GetWebContents(),
      autofill::ChromeAutofillClient::FromWebContents(
          web_view_->GetWebContents()));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_view_->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      web_view_->GetWebContents())
      ->SetDelegate(this);

  web_view_->LoadInitialURL(url);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::USER_MANAGER_PROFILE);
}

UserManagerProfileDialogDelegate::~UserManagerProfileDialogDelegate() = default;

gfx::Size UserManagerProfileDialogDelegate::CalculatePreferredSize() const {
  return gfx::Size(UserManagerProfileDialog::kDialogWidth,
                   UserManagerProfileDialog::kDialogHeight);
}

void UserManagerProfileDialogDelegate::DisplayErrorMessage() {
  web_view_->LoadInitialURL(GURL(chrome::kChromeUISigninErrorURL));
}

web_modal::WebContentsModalDialogHost*
UserManagerProfileDialogDelegate::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView UserManagerProfileDialogDelegate::GetHostView() const {
  return GetWidget()->GetNativeView();
}

gfx::Point UserManagerProfileDialogDelegate::GetDialogPosition(
    const gfx::Size& size) {
  gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  return gfx::Point(std::max(0, (widget_size.width() - size.width()) / 2),
                    std::max(0, (widget_size.height() - size.height()) / 2));
}

gfx::Size UserManagerProfileDialogDelegate::GetMaximumDialogSize() {
  return GetWidget()->GetWindowBoundsInScreen().size();
}

void UserManagerProfileDialogDelegate::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {}

void UserManagerProfileDialogDelegate::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {}

ui::ModalType UserManagerProfileDialogDelegate::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

void UserManagerProfileDialogDelegate::DeleteDelegate() {
  OnDialogDestroyed();
  delete this;
}

views::View* UserManagerProfileDialogDelegate::GetInitiallyFocusedView() {
  return static_cast<views::View*>(web_view_);
}

void UserManagerProfileDialogDelegate::CloseDialog() {
  OnDialogDestroyed();
  GetWidget()->Close();
}

void UserManagerProfileDialogDelegate::OnDialogDestroyed() {
  if (host_) {
    host_->OnDialogDestroyed();
    host_ = nullptr;
  }
}
