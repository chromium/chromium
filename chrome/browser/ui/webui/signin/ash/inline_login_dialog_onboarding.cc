// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog_onboarding.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

InlineLoginDialogOnboarding::Delegate::Delegate(
    InlineLoginDialogOnboarding* dialog)
    : dialog_(dialog->GetWeakPtr()) {
  widget_ = base::WrapUnique(
      views::Widget::GetWidgetForNativeView(dialog->GetHostView()));
  widget_->MakeCloseSynchronous(base::BindOnce(
      [](Delegate* delegate, views::Widget::ClosedReason reason) {
        delegate->widget_.reset();
        delegate->dialog_ = nullptr;
      },
      base::Unretained(this)));
}

InlineLoginDialogOnboarding::Delegate::~Delegate() {
  CloseWithoutCallback();
}

void InlineLoginDialogOnboarding::Delegate::CloseWithoutCallback() {
  if (dialog_) {
    dialog_->dialog_closed_callback_.Reset();
    dialog_->Close();
  }
}

void InlineLoginDialogOnboarding::Delegate::Close() {
  if (dialog_) {
    dialog_->Close();
  }
}

void InlineLoginDialogOnboarding::Delegate::UpdateDialogBounds(
    const gfx::Rect& new_bounds) {
  if (dialog_) {
    dialog_->UpdateDialogBounds(new_bounds);
  }
}

// static
InlineLoginDialogOnboarding* InlineLoginDialogOnboarding::Show(
    const gfx::Size& size,
    gfx::NativeWindow window,
    base::OnceCallback<void(void)> dialog_closed_callback) {
  DCHECK(ProfileManager::GetActiveUserProfile()->IsChild());

  base::UmaHistogramEnumeration(
      account_manager::AccountManagerFacade::kAccountAdditionSource,
      ::account_manager::AccountManagerFacade::AccountAdditionSource::
          kOnboarding);

  DCHECK(window);

  auto* dialog =
      new InlineLoginDialogOnboarding(size, std::move(dialog_closed_callback));
  dialog->ShowSystemDialog(window);

  return dialog;
}

ui::mojom::ModalType InlineLoginDialogOnboarding::GetDialogModalType() const {
  // Override the default system-modal behavior of the dialog so that the
  // shelf can be accessed during onboarding.
  return ui::mojom::ModalType::kWindow;
}

InlineLoginDialogOnboarding::InlineLoginDialogOnboarding(
    const gfx::Size& size,
    base::OnceCallback<void(void)> dialog_closed_callback)
    : size_(size), dialog_closed_callback_(std::move(dialog_closed_callback)) {
  set_dialog_modal_type(ui::mojom::ModalType::kChild);
}

InlineLoginDialogOnboarding::~InlineLoginDialogOnboarding() = default;

void InlineLoginDialogOnboarding::UpdateDialogBounds(const gfx::Rect& bounds) {
  size_ = bounds.size();
  dialog_window()->SetBounds(bounds);
}

void InlineLoginDialogOnboarding::GetDialogSize(gfx::Size* size) const {
  *size = size_;
}

void InlineLoginDialogOnboarding::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  InlineLoginDialog::AdjustWidgetInitParams(params);
  params->type = views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS;
  params->ownership = views::Widget::InitParams::CLIENT_OWNS_WIDGET;
}

void InlineLoginDialogOnboarding::OnDialogClosed(
    const std::string& json_retval) {
  if (dialog_closed_callback_) {
    std::move(dialog_closed_callback_).Run();
  }

  InlineLoginDialog::OnDialogClosed(json_retval);
}

base::WeakPtr<InlineLoginDialogOnboarding>
InlineLoginDialogOnboarding::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
