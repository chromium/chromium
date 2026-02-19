// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_auto_signin_toast_delegate.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/menus/simple_menu_model.h"

ManagePasswordsAutoSigninToastDelegate::ManagePasswordsAutoSigninToastDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ManagePasswordsAutoSigninToastDelegate::
    ~ManagePasswordsAutoSigninToastDelegate() = default;

void ManagePasswordsAutoSigninToastDelegate::ExecuteCommand(int command_id,
                                                            int event_flags) {
  if (command_id == kAutoSignInOpenPasswordManagerSettingsCommand) {
    if (auto* tab_interface =
            tabs::TabInterface::MaybeGetFromContents(web_contents_)) {
      BrowserWindowInterface* browser =
          tab_interface->GetBrowserWindowInterface();
      if (browser) {
        NavigateToPasswordManagerSettings(browser);
      }
    }
  }
}

void ManagePasswordsAutoSigninToastDelegate::SetOnToastClosedCallback(
    base::OnceClosure on_toast_closed_callback) {
  on_toast_closed_callback_ = std::move(on_toast_closed_callback);
}

void ManagePasswordsAutoSigninToastDelegate::OnAutoSignInToast(
    const std::u16string& username) {
  ToastController* toast_controller = GetToastController();
  if (!toast_controller) {
    return;
  }
  toast_observation_ =
      toast_controller->RegisterOnWidgetDestroyed(base::BindRepeating(
          &ManagePasswordsAutoSigninToastDelegate::OnToastWidgetDestroyed,
          base::Unretained(this)));

  if (username.empty()) {
    return;
  }

  ToastParams params(ToastId::kAutoSignIn);
  params.body_string_replacement_params.push_back(username);
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddItemWithStringIdAndIcon(
      kAutoSignInOpenPasswordManagerSettingsCommand, IDS_MANAGE,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                     ui::kColorMenuIcon, 16));
  params.menu_model = std::move(menu_model);
  toast_controller->MaybeShowToast(std::move(params));
}

ToastController* ManagePasswordsAutoSigninToastDelegate::GetToastController() {
  return ToastController::MaybeGetForWebContents(web_contents_);
}

void ManagePasswordsAutoSigninToastDelegate::NavigateToPasswordManagerSettings(
    BrowserWindowInterface* browser) {
  chrome::ShowPasswordManagerSettings(browser);
}

void ManagePasswordsAutoSigninToastDelegate::OnToastWidgetDestroyed(
    ToastId toast_id) {
  if (toast_id == ToastId::kAutoSignIn && on_toast_closed_callback_) {
    std::move(on_toast_closed_callback_).Run();
  }
}
