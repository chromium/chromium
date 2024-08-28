// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_controller.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_features.h"

ToastParams::ToastParams(ToastId id) : toast_id_(id) {}
ToastParams::ToastParams(ToastParams&& other) noexcept = default;
ToastParams& ToastParams::operator=(ToastParams&& other) noexcept = default;
ToastParams::~ToastParams() = default;

ToastController::ToastController(
    BrowserWindowInterface* browser_window_interface,
    const ToastRegistry* toast_registry)
    : browser_window_interface_(browser_window_interface),
      toast_registry_(toast_registry) {}

// TODO(crbug.com/358610787): ensure that no toast is showing when the
// destructor is called.
ToastController::~ToastController() = default;

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

  // TODO(crbug.com/358610190): show the toast and manage resetting/stopping the
  // timer when a toast is shown or closed.
  return true;
}

void ToastController::ClosePersistentToast(ToastId id) {
  CHECK(current_toast_params_.has_value());
  CHECK_EQ(current_toast_params_.value().toast_id_, id);
  // TODO(crbug.com/358610787): close the persistent toast and have internal
  // state reflect that.
  CloseToast();
}

void ToastController::CloseToast() {
  current_toast_params_ = std::nullopt;
  // TODO(crbug.com/358610190): close the currently showing toast.
}
