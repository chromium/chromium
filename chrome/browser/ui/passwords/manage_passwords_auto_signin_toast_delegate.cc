// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_auto_signin_toast_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"

namespace {

class AutoSignInToastMenuModel : public ui::SimpleMenuModel::Delegate,
                                 public ui::SimpleMenuModel {
 public:
  explicit AutoSignInToastMenuModel(
      base::WeakPtr<ManagePasswordsAutoSigninToastDelegate> delegate)
      : ui::SimpleMenuModel(/*delegate=*/this), delegate_(std::move(delegate)) {
    AddItemWithStringIdAndIcon(
        ManagePasswordsAutoSigninToastDelegate::
            kAutoSignInOpenPasswordManagerSettingsCommand,
        IDS_MANAGE,
        ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                           ? vector_icons::kSettingsFilledIcon
                                           : vector_icons::kSettingsOldIcon,
                                       ui::kColorMenuIcon,
                                       ui::SimpleMenuModel::kDefaultIconSize));
    SetElementIdentifierAt(0, ManagePasswordsAutoSigninToastDelegate::
                                  kOpenPasswordManagerSettingsMenuItem);
  }
  AutoSignInToastMenuModel(const AutoSignInToastMenuModel&) = delete;
  AutoSignInToastMenuModel& operator=(const AutoSignInToastMenuModel&) = delete;
  ~AutoSignInToastMenuModel() override = default;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (delegate_) {
      delegate_->ExecuteCommand(command_id, event_flags);
    }
  }

 private:
  base::WeakPtr<ManagePasswordsAutoSigninToastDelegate> delegate_;
};

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ManagePasswordsAutoSigninToastDelegate,
                                      kOpenPasswordManagerSettingsMenuItem);

ManagePasswordsAutoSigninToastDelegate::ManagePasswordsAutoSigninToastDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr) {}

ManagePasswordsAutoSigninToastDelegate::
    ~ManagePasswordsAutoSigninToastDelegate() = default;

void ManagePasswordsAutoSigninToastDelegate::ExecuteCommand(int command_id,
                                                            int event_flags) {
  if (!web_contents_) {
    return;
  }
  if (command_id == kAutoSignInOpenPasswordManagerSettingsCommand) {
    if (auto* tab_interface =
            tabs::TabInterface::MaybeGetFromContents(web_contents_.get())) {
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
  if (username.empty()) {
    return;
  }
  ToastController* toast_controller = GetToastController();
  if (!toast_controller) {
    return;
  }
  toast_observation_ =
      toast_controller->RegisterOnWidgetDestroyed(base::BindRepeating(
          &ManagePasswordsAutoSigninToastDelegate::OnToastWidgetDestroyed,
          weak_ptr_factory_.GetWeakPtr()));

  ToastParams params(ToastId::kAutoSignIn);
  params.body_string_replacement_params.push_back(username);
  params.menu_model = std::make_unique<AutoSignInToastMenuModel>(
      weak_ptr_factory_.GetWeakPtr());
  toast_controller->MaybeShowToast(std::move(params));
}

ToastController* ManagePasswordsAutoSigninToastDelegate::GetToastController() {
  return web_contents_
             ? ToastController::MaybeGetForWebContents(web_contents_.get())
             : nullptr;
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
