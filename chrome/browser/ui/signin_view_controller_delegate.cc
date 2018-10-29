// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin_view_controller_delegate.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"

namespace {

content::WebContents* GetAuthFrameWebContents(
    content::WebContents* web_ui_web_contents) {
  return signin::GetAuthFrameWebContents(web_ui_web_contents, "signin-frame");
}

}  // namespace

SigninViewControllerDelegate::SigninViewControllerDelegate(
    SigninViewController* signin_view_controller,
    content::WebContents* web_contents,
    Browser* browser)
    : signin_view_controller_(signin_view_controller),
      web_contents_(web_contents),
      browser_(browser) {
  DCHECK(web_contents_);
  DCHECK(browser_);
  DCHECK(browser_->tab_strip_model()->GetActiveWebContents())
      << "A tab must be active to present the sign-in modal dialog.";
  web_contents_->SetDelegate(this);
}

SigninViewControllerDelegate::~SigninViewControllerDelegate() {}

void SigninViewControllerDelegate::AttachDialogManager() {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents_);
  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  manager->SetDelegate(this);
}

void SigninViewControllerDelegate::CloseModalSignin() {
  ResetSigninViewControllerDelegate();
  PerformClose();
}

void SigninViewControllerDelegate::PerformNavigation() {
  if (CanGoBack(web_contents_))
    GetAuthFrameWebContents(web_contents_)->GetController().GoBack();
  else
    CloseModalSignin();
}

bool SigninViewControllerDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Discard the context menu
  return true;
}

web_modal::WebContentsModalDialogHost*
SigninViewControllerDelegate::GetWebContentsModalDialogHost() {
  return browser()->window()->GetWebContentsModalDialogHost();
}

void SigninViewControllerDelegate::ResetSigninViewControllerDelegate() {
  if (signin_view_controller_) {
    signin_view_controller_->ResetModalSigninDelegate();
    signin_view_controller_ = nullptr;
  }
}

// content::WebContentsDelegate
void SigninViewControllerDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool to_different_document) {
  // The WebUI object can be missing for an error page, per
  // https://crbug.com/860409.
  if (!source->GetWebUI())
    return;

  if (CanGoBack(source)) {
    source->GetWebUI()->CallJavascriptFunctionUnsafe(
        "inline.login.showBackButton");
  } else {
    source->GetWebUI()->CallJavascriptFunctionUnsafe(
        "inline.login.showCloseButton");
  }
}

bool SigninViewControllerDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  NOTREACHED();
  return false;
}

bool SigninViewControllerDelegate::CanGoBack(
    content::WebContents* web_ui_web_contents) const {
  auto* auth_web_contents = GetAuthFrameWebContents(web_ui_web_contents);
  return auth_web_contents && auth_web_contents->GetController().CanGoBack();
}
