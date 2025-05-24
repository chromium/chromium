// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_controller.h"

#include <memory>
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_metrics.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/common/pref_names.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

ToastParams::ToastParams(ToastId id) : toast_id(id) {}
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

bool ToastController::CanShowToast(ToastId toast_id) const {
  if (static_cast<toasts::ToastAlertLevel>(
          g_browser_process->local_state()->GetInteger(
              prefs::kToastAlertLevel)) ==
      toasts::ToastAlertLevel::kActionable) {
    const ToastSpecification* toast_spec =
        toast_registry_->GetToastSpecification(toast_id);
    return toast_spec->is_actionable();
  }
  return true;
}

std::optional<ToastId> ToastController::GetCurrentToastId() const {
  return currently_showing_toast_id_;
}

bool ToastController::MaybeShowToast(ToastParams params) {
  if (!CanShowToast(params.toast_id)) {
    RecordToastFailedToShow(params.toast_id);
    return false;
  }

  RecordToastTriggeredToShow(params.toast_id);
  CloseToast(toasts::ToastCloseReason::kPreempted);

  if (IsShowingToast()) {
    QueueToast(std::move(params));
  } else {
    ShowToast(std::move(params));
  }

  return true;
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
    next_toast_params_ = std::nullopt;
    omnibox_helper_observer_.Reset();
  }

  if (next_toast_params_.has_value()) {
    ShowToast(std::move(next_toast_params_.value()));
    next_toast_params_ = std::nullopt;
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
  next_toast_params_ = std::move(params);
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
      toast_registry_->GetToastSpecification(params.toast_id);
  CHECK(current_toast_spec);
  CHECK_EQ(current_toast_spec->has_menu(), !!params.menu_model);
  CHECK(current_toast_spec->body_string_id() != 0 ||
        params.body_string_override.has_value());
  CHECK(params.body_string_replacement_params.empty() ||
        !params.body_string_cardinality_param.has_value());

  currently_showing_toast_id_ = params.toast_id;
  const bool is_actionable =
      current_toast_spec->action_button_string_id().has_value() ||
      current_toast_spec->has_menu();
  base::TimeDelta timeout =
      is_actionable ? kToastWithActionTimeout : kToastDefaultTimeout;

  toast_close_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&ToastController::CloseToast, base::Unretained(this),
                     toasts::ToastCloseReason::kAutoDismissed));
  CreateToast(std::move(params), current_toast_spec);
}

void ToastController::CloseToast(toasts::ToastCloseReason reason) {
  if (toast_view_) {
    toast_view_->Close(reason);
  }
}

void ToastController::CreateToast(ToastParams params,
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
  const ui::ImageModel* image_override = params.image_override.has_value()
                                             ? &params.image_override.value()
                                             : nullptr;

  const std::u16string body_string =
      params.body_string_override.has_value()
          ? params.body_string_override.value()
          : FormatString(spec->body_string_id(),
                         params.body_string_replacement_params,
                         params.body_string_cardinality_param);
  auto toast_view = std::make_unique<toasts::ToastView>(
      anchor_view, body_string, spec->icon(), image_override,
      ShouldRenderToastOverWebContents(),
      base::BindRepeating(&RecordToastDismissReason, params.toast_id));

  if (spec->has_close_button()) {
    toast_view->AddCloseButton(
        base::BindRepeating(&RecordToastCloseButtonClicked, params.toast_id));
  }

  if (spec->action_button_string_id().has_value()) {
    toast_view->AddActionButton(
        FormatString(spec->action_button_string_id().value(),
                     params.action_button_string_replacement_params,
                     std::nullopt),
        spec->action_button_callback().Then(base::BindRepeating(
            &RecordToastActionButtonClicked, params.toast_id)));
  }

  if (spec->has_menu()) {
    toast_view->AddMenu(std::move(params.menu_model));
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
    std::vector<std::u16string> replacements,
    std::optional<int> cardinality) {
  if (cardinality.has_value()) {
    return l10n_util::GetPluralStringFUTF16(string_id, cardinality.value());
  } else {
    return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
  }
}

void ToastController::OnFullscreenStateChanged() {
  toast_view_->UpdateRenderToastOverWebContentsAndPaint(
      ShouldRenderToastOverWebContents());
}

void ToastController::ClearTabScopedToasts() {
  if (next_toast_params_.has_value()) {
    const ToastId toast_id = next_toast_params_.value().toast_id;
    const ToastSpecification* const specification =
        toast_registry_->GetToastSpecification(toast_id);
    RecordToastDismissReason(toast_id, toasts::ToastCloseReason::kAbort);
    if (!specification->is_global_scope()) {
      next_toast_params_ = std::nullopt;
    }
  }

  if (currently_showing_toast_id_.has_value() &&
      !toast_registry_
           ->GetToastSpecification(currently_showing_toast_id_.value())
           ->is_global_scope()) {
    toast_close_timer_.Stop();
    CloseToast(toasts::ToastCloseReason::kAbort);
  }
}
