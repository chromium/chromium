// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_controller.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "ui/base/l10n/l10n_util.h"

ToastParams::ToastParams(ToastId id) : toast_id_(id) {}
ToastParams::ToastParams(ToastParams&& other) noexcept = default;
ToastParams& ToastParams::operator=(ToastParams&& other) noexcept = default;
ToastParams::~ToastParams() = default;

ToastController::ToastController(
    BrowserWindowInterface* browser_window_interface,
    const ToastRegistry* toast_registry)
    : browser_window_interface_(browser_window_interface),
      toast_registry_(toast_registry) {}

ToastController::~ToastController() {
  CloseToast();
}

bool ToastController::IsShowingToast() const {
  return current_toast_params_.has_value();
}

bool ToastController::CanShowToast(ToastId id) const {
  if (!base::FeatureList::IsEnabled(toast_features::kToastFramework)) {
    return false;
  }

  if (!IsShowingToast()) {
    return true;
  }

  const ToastSpecification* current_toast_spec =
      toast_registry_->GetToastSpecification(
          current_toast_params_.value().toast_id_);
  const ToastSpecification* potential_toast_spec =
      toast_registry_->GetToastSpecification(id);

  return !(current_toast_spec->is_persistent_toast() &&
           potential_toast_spec->is_persistent_toast());
}

bool ToastController::MaybeShowToast(ToastParams params) {
  if (!CanShowToast(params.toast_id_)) {
    return false;
  }

  CloseToast();

  if (IsShowingToast()) {
    // TODO(crbug.com/358610190): Record that a toast was preempted if
    // `next_toast_params_` already have a value.

    // Queue the params since we are waiting for the previous toast to
    // fully close before showing the next toast.
    next_toast_params_ = std::move(params);
  } else {
    ShowToast(std::move(params));
  }

  return true;
}

void ToastController::ClosePersistentToast(ToastId id) {
  CHECK(current_toast_params_.has_value());
  CHECK_EQ(current_toast_params_.value().toast_id_, id);
  // TODO(crbug.com/358610787): close the persistent toast and have internal
  // state reflect that.
  CloseToast();
}

void ToastController::OnWidgetDestroyed(views::Widget* widget) {
  current_toast_params_ = std::nullopt;
  toast_observer_.Reset();
  toast_widget_ = nullptr;
  toast_close_timer_.Stop();

  // TODO(crbug.com/358610190): Record toast closed reason.

  // The previous toast is destroyed so we should show the next toast.
  if (next_toast_params_.has_value()) {
    ShowToast(std::move(next_toast_params_.value()));
    next_toast_params_ = std::nullopt;
  }
}

void ToastController::ShowToast(ToastParams params) {
  current_toast_params_ = std::move(params);

  const ToastSpecification* current_toast_spec =
      toast_registry_->GetToastSpecification(
          current_toast_params_.value().toast_id_);

  CHECK(current_toast_spec);
  if (!current_toast_spec->is_persistent_toast()) {
    toast_close_timer_.Start(
        FROM_HERE, toast_features::kToastTimeout.Get(),
        base::BindOnce(&ToastController::CloseToast, base::Unretained(this)));
  }

  CreateToast(current_toast_spec);
}

void ToastController::CloseToast() {
  if (!IsShowingToast()) {
    return;
  }

  CHECK(toast_widget_);
  // TODO(crbug.com/358615317): Make the toast animate out and then complete
  // the rest of the logic synchronously afterwards.
  // TODO(crbug.com/358610872): Log toast close reason metric and potentially
  // integrate with Widget::CloseReason.
  toast_widget_->Close();
}

void ToastController::CreateToast(const ToastSpecification* spec) {
  if (!browser_window_interface_) {
    CHECK_IS_TEST();
    return;
  }
  CHECK(current_toast_params_.has_value());
  std::unique_ptr<toasts::ToastView> toast =
      std::make_unique<toasts::ToastView>(
          browser_window_interface_->TopContainer(),
          FormatString(spec->body_string_id(),
                       current_toast_params_->body_string_replacement_params_),
          spec->icon());
  toast_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(std::move(toast));
  toast_observer_.Observe(toast_widget_);
  toast_widget_->ShowInactive();
  // TODO(crbug.com/358615317): Make the toast animate in.
}

std::u16string ToastController::FormatString(
    int string_id,
    std::vector<std::u16string> replacements) {
  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
