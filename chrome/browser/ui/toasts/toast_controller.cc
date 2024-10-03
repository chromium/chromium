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
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_metrics.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

ToastParams::ToastParams(ToastId id) : toast_id_(id) {}
ToastParams::ToastParams(ToastParams&& other) noexcept = default;
ToastParams& ToastParams::operator=(ToastParams&& other) noexcept = default;
ToastParams::~ToastParams() = default;

ToastController::ToastController(
    BrowserWindowInterface* browser_window_interface,
    const ToastRegistry* toast_registry)
    : browser_window_interface_(browser_window_interface),
      toast_registry_(toast_registry) {}

ToastController::~ToastController() = default;

void ToastController::Init() {
  CHECK(browser_window_interface_);
  CHECK(browser_subscriptions_.empty());
  browser_subscriptions_.push_back(
      browser_window_interface_->RegisterActiveTabDidChange(base::BindRepeating(
          &ToastController::OnActiveTabChanged, base::Unretained(this))));
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

  RecordToastTriggeredToShow(params.toast_id_);
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
    // Clears the stored focus view so that after widget activation occurs,
    // focus will not advance out of the widget and into the ContentsWebView.
    toast_widget_->GetFocusManager()->SetStoredFocusView(nullptr);
  } else {
    // On Mac, traversing out of the toast widget and into the browser causes
    // the browser to advance focus twice so we clear the focus to achieve the
    // expected focus behavior.
    browser_window_interface_->TopContainer()
        ->GetWidget()
        ->GetFocusManager()
        ->ClearFocus();
  }
}
#endif

void ToastController::OnWidgetDestroyed(views::Widget* widget) {
  current_ephemeral_params_ = std::nullopt;
  currently_showing_toast_id_ = std::nullopt;
  toast_view_ = nullptr;
  toast_widget_ = nullptr;
  toast_observer_.Reset();
  fullscreen_observation_.Reset();
  toast_close_timer_.Stop();

  if (browser_window_interface_ &&
      browser_window_interface_->IsAttemptingToCloseBrowser()) {
    // Clear any queued toasts to prevent them from showing
    // after an existing toast is destroyed while the browser is trying to
    // close.
    next_ephemeral_params_ = std::nullopt;
    persistent_params_ = std::nullopt;
    omnibox_helper_observer_.Reset();
  }

  if (next_ephemeral_params_.has_value()) {
    ShowToast(std::move(next_ephemeral_params_.value()));
    next_ephemeral_params_ = std::nullopt;
  } else if (persistent_params_.has_value()) {
    ShowToast(std::move(persistent_params_.value()));
  }
}

void ToastController::PrimaryPageChanged(content::Page& page) {
  ClearTabScopedToasts();
}

base::OneShotTimer* ToastController::GetToastCloseTimerForTesting() {
  return &toast_close_timer_;
}

void ToastController::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  tabs::TabInterface* const tab_interface =
      browser_interface->GetActiveTabInterface();
  content::WebContents* const web_contents = tab_interface->GetContents();
  OmniboxTabHelper* const tab_helper =
      OmniboxTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  omnibox_helper_observer_.Reset();
  omnibox_helper_observer_.Observe(tab_helper);
  Observe(web_contents);
  ClearTabScopedToasts();
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

void ToastController::OnOmniboxInputInProgress(bool in_progress) {
  if (in_progress) {
    UpdateToastWidgetVisibility(false);
  }
}

void ToastController::OnOmniboxFocusChanged(OmniboxFocusState state,
                                            OmniboxFocusChangeReason reason) {
  UpdateToastWidgetVisibility(state == OmniboxFocusState::OMNIBOX_FOCUS_NONE);
}

void ToastController::OnOmniboxPopupVisibilityChanged(bool popup_is_open) {
  is_omnibox_popup_showing_ = popup_is_open;
  UpdateToastWidgetVisibility(!popup_is_open);
}

void ToastController::UpdateToastWidgetVisibility(bool show_toast_widget) {
  if (toast_widget_) {
    if (show_toast_widget) {
      toast_widget_->ShowInactive();
    } else {
      toast_widget_->Hide();
    }
  }
}

bool ToastController::ShouldRenderToastOverWebContents() {
  bool render_in_contents =
      browser_window_interface_->ShouldHideUIForFullscreen();

#if BUILDFLAG(IS_MAC)
  render_in_contents |=
      fullscreen_utils::IsInContentFullscreen(browser_window_interface_);
#endif

  return render_in_contents;
}

void ToastController::WebContentsDestroyed() {
  omnibox_helper_observer_.Reset();
  Observe(nullptr);
}

void ToastController::ShowToast(ToastParams params) {
  // TODO(crbug.com/367755347): Remove check when test is fixed.
  CHECK(!toast_registry_->IsEmpty());
  const ToastSpecification* current_toast_spec =
      toast_registry_->GetToastSpecification(params.toast_id_);
  CHECK(current_toast_spec);

  currently_showing_toast_id_ = params.toast_id_;
  if (current_toast_spec->is_persistent_toast()) {
    persistent_params_ = std::move(params);
  } else {
    current_ephemeral_params_ = std::move(params);
    base::TimeDelta timeout =
        current_toast_spec->action_button_string_id().has_value()
            ? toast_features::kToastTimeout.Get()
            : toast_features::kToastWithoutActionTimeout.Get();

    toast_close_timer_.Start(
        FROM_HERE, timeout,
        base::BindOnce(&ToastController::CloseToast, base::Unretained(this),
                       toasts::ToastCloseReason::kAutoDismissed));
  }

  CreateToast(current_toast_spec->is_persistent_toast()
                  ? persistent_params_.value()
                  : current_ephemeral_params_.value(),
              current_toast_spec);
}

void ToastController::CloseToast(toasts::ToastCloseReason reason) {
  if (toast_view_) {
    toast_view_->Close(reason);
  }
}

void ToastController::CreateToast(const ToastParams& params,
                                  const ToastSpecification* spec) {
  // TODO(crbug.com/364730656): Replace this logic when improving
  // ToastController testability.
  if (browser_window_interface_ == nullptr ||
      !browser_window_interface_->TopContainer()) {
    // Don't actually create the toast in unit tests
    CHECK_IS_TEST();
    return;
  }

  views::View* const anchor_view = browser_window_interface_->TopContainer();
  CHECK(anchor_view);
  auto toast_view = std::make_unique<toasts::ToastView>(
      anchor_view,
      FormatString(spec->body_string_id(),
                   params.body_string_replacement_params_),
      spec->icon(), ShouldRenderToastOverWebContents(),
      base::BindRepeating(&RecordToastDismissReason, params.toast_id_));

  if (spec->has_close_button()) {
    toast_view->AddCloseButton(
        base::BindRepeating(&RecordToastCloseButtonClicked, params.toast_id_));
  }

  if (spec->action_button_string_id().has_value()) {
    toast_view->AddActionButton(
        FormatString(spec->action_button_string_id().value(),
                     params.action_button_string_replacement_params_),
        spec->action_button_callback().Then(base::BindRepeating(
            &RecordToastActionButtonClicked, params.toast_id_)));
  }

  toast_view_ = toast_view.get();
  toast_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(std::move(toast_view));
  // Get rid of the border that is drawn by default when we set the toast to
  // have a shadow.
  toast_view_->GetBubbleFrameView()->bubble_border()->set_draw_border_stroke(
      false);
  toast_observer_.Observe(toast_widget_);
  fullscreen_observation_.Observe(
      browser_window_interface_->GetExclusiveAccessManager()
          ->fullscreen_controller());
  toast_widget_->SetVisibilityChangedAnimationsEnabled(false);
  // Set the the focus traversable parent of the toast widget to be the parent
  // of the anchor view, so that when focus leaves the toast, the search for the
  // next focusable view will start from the right place. However, does not set
  // the anchor view's focus traversable to be the toast widget, because when
  // focus leaves the toast widget it will go into the anchor view's focus
  // traversable if it exists, so doing that would trap focus inside of the
  // toast widget.
  toast_widget_->SetFocusTraversableParent(
      anchor_view->parent()->GetWidget()->GetFocusTraversable());
  toast_widget_->SetFocusTraversableParentView(anchor_view);

  if (!is_omnibox_popup_showing_) {
    toast_widget_->ShowInactive();
    toast_view_->AnimateIn();
  } else {
    toast_widget_->Hide();
  }
}

std::u16string ToastController::FormatString(
    int string_id,
    std::vector<std::u16string> replacements) {
  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

void ToastController::OnFullscreenStateChanged() {
  toast_view_->UpdateRenderToastOverWebContentsAndPaint(
      ShouldRenderToastOverWebContents());
}

void ToastController::ClearTabScopedToasts() {
  toast_close_timer_.Stop();
  if (next_ephemeral_params_.has_value()) {
    const ToastSpecification* const specification =
        toast_registry_->GetToastSpecification(
            next_ephemeral_params_.value().toast_id_);
    if (!specification->is_global_scope()) {
      next_ephemeral_params_ = std::nullopt;
    }
    RecordToastDismissReason(next_ephemeral_params_.value().toast_id_,
                             toasts::ToastCloseReason::kAbort);
  }

  if (current_ephemeral_params_.has_value()) {
    const ToastSpecification* const specification =
        toast_registry_->GetToastSpecification(
            current_ephemeral_params_.value().toast_id_);
    if (!specification->is_global_scope()) {
      CloseToast(toasts::ToastCloseReason::kAbort);
    }
  }
}
