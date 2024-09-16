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
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

ToastParams::ToastParams(ToastId id) : toast_id_(id) {}
ToastParams::ToastParams(ToastParams&& other) noexcept = default;
ToastParams& ToastParams::operator=(ToastParams&& other) noexcept = default;
ToastParams::~ToastParams() = default;

ToastController::ToastController(
    BrowserWindowInterface* browser_window_interface,
    const ToastRegistry* toast_registry)
    : browser_window_interface_(browser_window_interface),
      toast_registry_(toast_registry) {
  BrowserList::AddObserver(this);
}

ToastController::~ToastController() {
  BrowserList::RemoveObserver(this);
}

bool ToastController::IsShowingToast() const {
  return GetCurrentToastId().has_value();
}

bool ToastController::CanShowToast(ToastId id) const {
  if (!base::FeatureList::IsEnabled(toast_features::kToastFramework)) {
    return false;
  }

  if (!IsShowingToast()) {
    return true;
  }

  const ToastSpecification* potential_toast_spec =
      toast_registry_->GetToastSpecification(id);

  return !(persistent_params_.has_value() &&
           potential_toast_spec->is_persistent_toast());
}

std::optional<ToastId> ToastController::GetCurrentToastId() const {
  return currently_showing_toast_id_;
}

bool ToastController::MaybeShowToast(ToastParams params) {
  if (!CanShowToast(params.toast_id_)) {
    return false;
  }

  CloseToast(toasts::ToastCloseReason::kPreempted);

  if (IsShowingToast()) {
    QueueToast(std::move(params));
  } else {
    ShowToast(std::move(params));
  }

  return true;
}

void ToastController::ClosePersistentToast(ToastId id) {
  CHECK(persistent_params_.has_value());
  CHECK_EQ(persistent_params_.value().toast_id_, id);
  std::optional<ToastId> current_toast_id = GetCurrentToastId();
  persistent_params_ = std::nullopt;

  // Close the toast if we are currently showing a persistent toast.
  if (current_toast_id.has_value() &&
      toast_registry_->GetToastSpecification(current_toast_id.value())
          ->is_persistent_toast()) {
    CloseToast(toasts::ToastCloseReason::kFeatureDismiss);
  }
}

#if BUILDFLAG(IS_MAC)
void ToastController::OnWidgetActivationChanged(views::Widget* widget,
                                                bool active) {
  if (active) {
    // On Mac, traversing out of the widget into the browser causes the browser
    // to restore its focus to the wrong place. Thus, when entering the toast
    // widget, make sure to clear out the browser's native focus. This causes
    // the toast widget to lose activation, so reactivate it manually.
    browser_window_interface_->TopContainer()
        ->GetWidget()
        ->GetFocusManager()
        ->ClearNativeFocus();
    toast_->GetWidget()->Activate();
  }
}
#endif

void ToastController::OnWidgetDestroyed(views::Widget* widget) {
  current_ephemeral_params_ = std::nullopt;
  currently_showing_toast_id_ = std::nullopt;
  toast_ = nullptr;
  toast_observer_.Reset();
  fullscreen_observation_.Reset();
  toast_close_timer_.Stop();

  if (next_ephemeral_params_.has_value()) {
    ShowToast(std::move(next_ephemeral_params_.value()));
    next_ephemeral_params_ = std::nullopt;
  } else if (persistent_params_.has_value()) {
    ShowToast(std::move(persistent_params_.value()));
  }
}

void ToastController::OnBrowserClosing(Browser* browser) {
  // Clear any queued toasts to prevent them from showing
  // after an existing toast is destroyed.
  if (browser_window_interface_ == browser) {
    next_ephemeral_params_ = std::nullopt;
    persistent_params_ = std::nullopt;
  }
}

base::OneShotTimer* ToastController::GetToastCloseTimerForTesting() {
  return &toast_close_timer_;
}

void ToastController::QueueToast(ToastParams params) {
  if (next_ephemeral_params_.has_value()) {
    // TODO(crbug.com/358610190): Record that next_ephemeral_params_ was
    // preempted.
    next_ephemeral_params_ = std::nullopt;
  } else if (persistent_params_.has_value()) {
    // TODO(crbug.com/358610190): Record that persistent_params_ was
    // preempted.
  } else {
    // Since we are queuing a toast, current_ephemeral_params must have a value
    // if we do not already have another ephemeral toast queued up.
    CHECK(current_ephemeral_params_.has_value());
    // TODO(crbug.com/358610190): Record that current_ephemeral_params was
    // preempted.
  }

  if (toast_registry_->GetToastSpecification(params.toast_id_)
          ->is_persistent_toast()) {
    CHECK(!persistent_params_.has_value());
    persistent_params_ = std::move(params);
  } else {
    next_ephemeral_params_ = std::move(params);
  }
}

void ToastController::ShowToast(ToastParams params) {
  const ToastSpecification* current_toast_spec =
      toast_registry_->GetToastSpecification(params.toast_id_);
  CHECK(current_toast_spec);

  currently_showing_toast_id_ = params.toast_id_;
  if (current_toast_spec->is_persistent_toast()) {
    persistent_params_ = std::move(params);
  } else {
    current_ephemeral_params_ = std::move(params);
    toast_close_timer_.Start(
        FROM_HERE, toast_features::kToastTimeout.Get(),
        base::BindOnce(&ToastController::CloseToast, base::Unretained(this),
                       toasts::ToastCloseReason::kAutoDismissed));
  }

  CreateToast(current_toast_spec->is_persistent_toast()
                  ? persistent_params_.value()
                  : current_ephemeral_params_.value(),
              current_toast_spec);
}

void ToastController::CloseToast(toasts::ToastCloseReason reason) {
  if (toast_) {
    toast_->Close(reason);
  }
}

void ToastController::CreateToast(const ToastParams& params,
                                  const ToastSpecification* spec) {
  views::View* anchor_view = browser_window_interface_->TopContainer();
  CHECK(anchor_view);
  auto toast_view = std::make_unique<toasts::ToastView>(
      anchor_view,
      FormatString(spec->body_string_id(),
                   params.body_string_replacement_params_),
      spec->icon(), spec->has_close_button(),
      browser_window_interface_->ShouldHideUIForFullscreen());

  if (spec->action_button_string_id().has_value()) {
    toast_view->AddActionButton(
        FormatString(spec->action_button_string_id().value(),
                     params.action_button_string_replacement_params_),
        spec->action_button_callback());
  }
  toast_ = toast_view.get();
  views::Widget* const toast_widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(toast_view));
  toast_observer_.Observe(toast_widget);
  fullscreen_observation_.Observe(
      browser_window_interface_->GetExclusiveAccessManager()
          ->fullscreen_controller());
  toast_widget->SetVisibilityChangedAnimationsEnabled(false);
  // Set the the focus traversable parent of the toast widget to be the anchor
  // view, so that when focus leaves the toast, the search for the next
  // focusable view will start from the right place. However, does not set the
  // anchor view's focus traversable to be the toast widget, because when focus
  // leaves the toast widget it will go into the anchor view's focus traversable
  // if it exists, so doing that would trap focus inside of the toast widget.
  toast_widget->SetFocusTraversableParent(
      anchor_view->GetWidget()->GetFocusTraversable());
  toast_widget->SetFocusTraversableParentView(anchor_view);
  toast_widget->ShowInactive();
  toast_->AnimateIn();
}

std::u16string ToastController::FormatString(
    int string_id,
    std::vector<std::u16string> replacements) {
  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

void ToastController::OnFullscreenStateChanged() {
  toast_->UpdateRenderToastOverWebContentsAndPaint(
      browser_window_interface_->ShouldHideUIForFullscreen());
}
