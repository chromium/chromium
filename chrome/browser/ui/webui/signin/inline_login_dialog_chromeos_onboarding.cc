// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos_onboarding.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

InlineLoginDialogChromeOSOnboarding::Delegate::Delegate(
    InlineLoginDialogChromeOSOnboarding* dialog)
    : dialog_(dialog) {
  widget_ = views::Widget::GetWidgetForNativeView(dialog->GetHostView());
  widget_->AddObserver(this);
}

InlineLoginDialogChromeOSOnboarding::Delegate::~Delegate() {
  if (widget_) {
    widget_->RemoveObserver(this);
  }

  CloseWithoutCallback();
}

void InlineLoginDialogChromeOSOnboarding::Delegate::CloseWithoutCallback() {
  if (dialog_) {
    dialog_->dialog_closed_callback_.Reset();
    dialog_->Close();
  }
}

void InlineLoginDialogChromeOSOnboarding::Delegate::Close() {
  if (dialog_) {
    dialog_->Close();
  }
}

void InlineLoginDialogChromeOSOnboarding::Delegate::UpdateDialogBounds(
    const gfx::Rect& new_bounds) {
  if (dialog_) {
    dialog_->UpdateDialogBounds(new_bounds);
  }
}

void InlineLoginDialogChromeOSOnboarding::Delegate::OnWidgetClosing(
    views::Widget* widget) {
  if (!dialog_ || widget != widget_) {
    return;
  }

  widget->RemoveObserver(this);
  widget_ = nullptr;
  dialog_ = nullptr;
}

// static
InlineLoginDialogChromeOSOnboarding* InlineLoginDialogChromeOSOnboarding::Show(
    const gfx::Size& size,
    gfx::NativeWindow window,
    base::OnceCallback<void(void)> dialog_closed_callback) {
  DCHECK(ProfileManager::GetActiveUserProfile()->IsChild());
  DCHECK(base::FeatureList::IsEnabled(supervised_users::kEduCoexistenceFlowV2));

  base::UmaHistogramEnumeration(
      account_manager::AccountManagerFacade::kAccountAdditionSource,
      ::account_manager::AccountManagerFacade::AccountAdditionSource::
          kOnboarding);

  DCHECK(window);

  auto* dialog = new InlineLoginDialogChromeOSOnboarding(
      size, std::move(dialog_closed_callback));
  dialog->ShowSystemDialog(window);

  return dialog;
}

ui::ModalType InlineLoginDialogChromeOSOnboarding::GetDialogModalType() const {
  // Override the default system-modal behavior of the dialog so that the
  // shelf can be accessed during onboarding.
  return ui::ModalType::MODAL_TYPE_WINDOW;
}

InlineLoginDialogChromeOSOnboarding::InlineLoginDialogChromeOSOnboarding(
    const gfx::Size& size,
    base::OnceCallback<void(void)> dialog_closed_callback)
    : size_(size), dialog_closed_callback_(std::move(dialog_closed_callback)) {
  set_modal_type(ui::MODAL_TYPE_CHILD);
}

InlineLoginDialogChromeOSOnboarding::~InlineLoginDialogChromeOSOnboarding() =
    default;

void InlineLoginDialogChromeOSOnboarding::UpdateDialogBounds(
    const gfx::Rect& bounds) {
  size_ = bounds.size();
  dialog_window()->SetBounds(bounds);
}

void InlineLoginDialogChromeOSOnboarding::GetDialogSize(gfx::Size* size) const {
  *size = size_;
}

void InlineLoginDialogChromeOSOnboarding::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  InlineLoginDialogChromeOS::AdjustWidgetInitParams(params);
  params->type = views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS;
}

void InlineLoginDialogChromeOSOnboarding::OnDialogClosed(
    const std::string& json_retval) {
  if (dialog_closed_callback_) {
    std::move(dialog_closed_callback_).Run();
  }

  InlineLoginDialogChromeOS::OnDialogClosed(json_retval);
}

}  // namespace chromeos
