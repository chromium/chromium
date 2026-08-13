// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

#include <optional>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/compositor/compositor.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/native_widget.h"

namespace omnibox {
const void* kOmniboxWebUIPopupWidgetId = &kOmniboxWebUIPopupWidgetId;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OmniboxPopupPresenterBase,
                                      kRoundedResultsFrame);

OmniboxPopupPresenterBase::OmniboxPopupPresenterBase(
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate,
    OmniboxController* controller)
    : location_bar_(location_bar),
      presenter_delegate_(presenter_delegate),
      controller_(controller) {
  owned_omnibox_popup_webui_container_ =
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build();
}

OmniboxPopupPresenterBase::~OmniboxPopupPresenterBase() {
  ReleaseWidget();
}

void OmniboxPopupPresenterBase::Show() {
  if (IsShown()) {
    return;
  }

  if (GetWebUIContent()) {
    auto* permission_manager =
        permissions::PermissionRequestManager::FromWebContents(
            GetWebUIContent()->GetWebContents());
    if (permission_manager && !permission_observation_.IsObserving()) {
      permission_observation_.Observe(permission_manager);
    }
  }

  if (ShouldPreserveRequestedFocus()) {
    focus_requested_ = false;
  }

  // Drop stale visual state callbacks.
  visual_state_weak_factory_.InvalidateWeakPtrs();

  EnsureWidgetCreated();
  SynchronizePopupBounds();

  if (auto* content = GetWebUIContent()) {
    content->ShowUI();

    // Call WasShown to mark the WebContents as visible so that a frame will
    // eventually be produced that triggers the OnVisualStateReady callback.
    // This must be called prior to `LogResultToContentReadyMetric`. If the
    // WebContents is still technically hidden when the metric attempts to
    // register its `InsertVisualStateCallback`, the graphics pipeline will
    // immediately drop the callback, resulting in lost telemetry data.
    content->GetWebContents()->WasShown();

    auto show_request_time = base::TimeTicks::Now();
    auto timeout = ShouldDeferUntilVisualStateReady();
    base::TimeTicks result_ready_time =
        controller()->autocomplete_controller()->result().result_ready_time();
    if (timeout.has_value()) {
      is_deferred_ = true;
      content->GetWebContents()
          ->GetPrimaryMainFrame()
          ->InsertVisualStateCallback(
              base::BindOnce(&OmniboxPopupPresenterBase::OnVisualStateReady,
                             visual_state_weak_factory_.GetWeakPtr(),
                             show_request_time,
                             result_ready_time,
                             /*from_fallback=*/false));

      // Add a backup timer in case the visual state callback is never called.
      // The visual state callback should always be called, but this fallback
      // ensures that if that assumption is ever broken, the UI will eventually
      // be shown.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&OmniboxPopupPresenterBase::OnVisualStateReady,
                         visual_state_weak_factory_.GetWeakPtr(),
                         show_request_time,
                         result_ready_time,
                         /*from_fallback=*/true,
                         /*success=*/false),
          timeout.value());
    } else {
      content->GetWebContents()
          ->GetPrimaryMainFrame()
          ->InsertVisualStateCallback(
              base::BindOnce(&OmniboxPopupPresenterBase::OnVisualStateReady,
                             visual_state_weak_factory_.GetWeakPtr(),
                             show_request_time,
                             result_ready_time,
                             /*from_fallback=*/false));
      ShowWidget(show_request_time);
    }
  }
}

void OmniboxPopupPresenterBase::OnVisualStateReady(
    base::TimeTicks show_request_time,
    base::TimeTicks result_ready_time,
    bool from_fallback,
    bool /*success*/) {
  const bool is_first_show = !has_logged_first_content_ready_;
  LogResultToContentReadyMetric(result_ready_time, is_first_show);

  if (!is_deferred_) {
    has_logged_first_content_ready_ = true;
    return;
  }

  base::UmaHistogramBoolean(
      base::StrCat({GetPopupMetricPrefix(), ".ContentReady.FromTimeout"}),
      from_fallback);
  if (is_first_show) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {GetPopupMetricPrefix(), ".ContentReady.FromTimeout.FirstShow"}),
        from_fallback);
  }

  base::TimeDelta duration = base::TimeTicks::Now() - show_request_time;
  base::UmaHistogramTimes(
      base::StrCat({GetPopupMetricPrefix(), ".ContentReady.Duration"}),
      duration);
  if (is_first_show) {
    base::UmaHistogramTimes(base::StrCat({GetPopupMetricPrefix(),
                                          ".ContentReady.Duration.FirstShow"}),
                            duration);
  }

  has_logged_first_content_ready_ = true;
  is_deferred_ = false;
  visual_state_weak_factory_.InvalidateWeakPtrs();
  // Fall back to showing the widget even if success == false
  // so the UI state matches the requested visibility.
  ShowWidget(show_request_time);
}

void OmniboxPopupPresenterBase::ShowWidget(base::TimeTicks show_request_time) {
  if (ShouldPreserveRequestedFocus() &&
      (widget_->IsActive() || focus_requested_)) {
    widget_->Show();
  } else {
    widget_->ShowInactive();
  }
  // If the derived class requests hiding for the initial layout pass, make the
  // widget transparent until we receive a valid content height.
  if (ShouldHideForInitialLayout() && content_height_ == 1) {
    widget_->SetOpacity(0.0f);
  }
  widget_->GetCompositor()->RequestPresentationTimeForNextFrame(base::BindOnce(
      &OmniboxPopupPresenterBase::OnWidgetPresented,
      visual_state_weak_factory_.GetWeakPtr(), show_request_time));

  if (auto* content = GetWebUIContent()) {
    content->GetWebContents()->WasShown();
  }

  if (!ShouldPreserveRequestedFocus() || focus_requested_) {
    RequestFocus();
  }
}

void OmniboxPopupPresenterBase::RequestFocus() {
  if (ShouldPreserveRequestedFocus()) {
    focus_requested_ = true;
  }
  if (widget_ && ShouldReceiveFocus()) {
    widget_->Activate();
    if (auto* content = GetWebUIContent()) {
      content->RequestFocus();
      if (content->GetWebContents()) {
        content->GetWebContents()->Focus();
      }
    }
  }
}

void OmniboxPopupPresenterBase::LogResultToContentReadyMetric(
    base::TimeTicks result_ready_time,
    bool is_first_show) {
  if (result_ready_time.is_null()) {
    omnibox::LogResultToContentReadyEarlyExitReason(
        omnibox::ResultToContentReadyEarlyExitReason::kNoResultReadyTime,
        GetPopupMetricPrefix());
    return;
  }

  const base::TimeDelta delta = base::TimeTicks::Now() - result_ready_time;

  base::UmaHistogramTimes(
      base::StrCat({GetPopupMetricPrefix(), ".ResultToContentReadyPerShow"}),
      delta);

  if (is_first_show) {
    base::UmaHistogramTimes(base::StrCat({GetPopupMetricPrefix(),
                                          ".ResultToContentReadyOnFirstShow"}),
                            delta);
  }
}

void OmniboxPopupPresenterBase::OnWidgetPresented(
    base::TimeTicks show_request_time,
    const gfx::PresentationFeedback& feedback) {
  if (feedback.failed()) {
    return;
  }
  const base::TimeDelta delta = feedback.timestamp - show_request_time;
  base::UmaHistogramTimes(
      base::StrCat({GetPopupMetricPrefix(), ".ShowToPaint.Duration"}), delta);
  if (!has_logged_first_widget_paint_) {
    has_logged_first_widget_paint_ = true;
    base::UmaHistogramTimes(base::StrCat({GetPopupMetricPrefix(),
                                          ".ShowToPaint.Duration.FirstShow"}),
                            delta);
  }
}

void OmniboxPopupPresenterBase::Hide() {
  permission_observation_.Reset();
  is_prompt_showing_ = false;
  is_handling_prompt_dismissal_ = false;

  if (ShouldPreserveRequestedFocus()) {
    focus_requested_ = false;
  }
  is_deferred_ = false;
  // Drop stale visual state callbacks.
  visual_state_weak_factory_.InvalidateWeakPtrs();

  // Only close if UI DevTools settings allow.
  if (widget_ && widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
    widget_->Hide();
    if (auto* content = GetWebUIContent()) {
      if (base::FeatureList::IsEnabled(
              omnibox::kOmniboxWebUIPopupMarkAsHidden)) {
        content->GetWebContents()->WasHidden();
      }
      content->Clear();
    }
  }
}

bool OmniboxPopupPresenterBase::IsShown() const {
  return is_deferred_ || (widget_ && widget_->IsVisible());
}

void OmniboxPopupPresenterBase::OnContentHeightChanged(int content_height) {
  content_height_ = content_height;
  // Restore opacity once we receive a valid content height.
  if (ShouldHideForInitialLayout() && content_height_ > 1 && widget_) {
    widget_->SetOpacity(1.0f);
  }
  SynchronizePopupBounds();
}

void OmniboxPopupPresenterBase::SynchronizePopupBounds() {
  if (widget_) {
    // In unit tests, `location_bar_` may be null.
    if (!location_bar_) {
      gfx::Rect widget_bounds = widget_->GetRestoredBounds();
      widget_bounds.set_width(
          std::max(minimum_size_.width(), widget_bounds.width()));
      widget_bounds.set_height(
          std::max(minimum_size_.height(), widget_bounds.height()));
      widget_->SetBounds(widget_bounds);
      return;
    }

    // The width is known, and is the basis for consistent web content rendering
    // so width is specified exactly; then only height adjusts dynamically.
    gfx::Rect widget_bounds = location_bar_->BoundsInScreen();

    widget_bounds.Inset(
        -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
    if (ShouldShowLocationBarCutout()) {
      widget_bounds.set_height(widget_bounds.height() + content_height_);
    } else {
      widget_bounds.set_height(
          std::max(content_height_, widget_bounds.height()));
    }

    // Set width and height to at least their minimums, or if larger,
    // their calculated versions.
    widget_bounds.set_width(
        std::max(minimum_size_.width(), widget_bounds.width()));
    widget_bounds.set_height(
        std::max(minimum_size_.height(), widget_bounds.height()));

    // Expand the widget bounds to accommodate the shadow borders around the
    // content.
    widget_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
    widget_->SetBounds(widget_bounds);
  }
}

views::View* OmniboxPopupPresenterBase::GetUIContainer() const {
  if (owned_omnibox_popup_webui_container_) {
    return owned_omnibox_popup_webui_container_.get();
  }
  return GetResultsFrame()->GetContents();
}

views::View* OmniboxPopupPresenterBase::GetOuterView() {
  return GetResultsFrame();
}

OmniboxPopupWebUIBaseContent* OmniboxPopupPresenterBase::GetWebUIContent()
    const {
  return omnibox_popup_webui_content_;
}

void OmniboxPopupPresenterBase::SetWebUIContent(
    std::unique_ptr<OmniboxPopupWebUIBaseContent> webui_content) {
  omnibox_popup_webui_content_ =
      GetUIContainer()->AddChildView(std::move(webui_content));

  Observe(omnibox_popup_webui_content_->GetWebContents());
  EnsureWidgetCreated();
}

void OmniboxPopupPresenterBase::EnsureWidgetCreated() {
  if (widget_) {
    return;
  }
  views::Widget* parent_widget = presenter_delegate_->GetLocationBarWidget();
  widget_ = std::make_unique<ThemeCopyingWidget>(parent_widget);

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      ShouldReceiveFocus() ? views::Widget::InitParams::TYPE_WINDOW_FRAMELESS
                           : views::Widget::InitParams::TYPE_POPUP);
#if BUILDFLAG(IS_WIN)
  // On Windows use the software compositor to ensure that we don't block
  // the UI thread during command buffer creation. See http://crbug.com/40198772
  params.force_software_compositing = true;
#endif
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent_widget->GetNativeView();
  params.context = parent_widget->GetNativeWindow();

  RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_.get());

  widget_->MakeCloseSynchronous(base::BindOnce(
      &OmniboxPopupPresenterBase::OnWidgetClosed, base::Unretained(this)));

  widget_->Init(std::move(params));
  widget_->SetNativeWindowProperty(
      views::kWidgetIdentifierKey,
      const_cast<void*>(omnibox::kOmniboxWebUIPopupWidgetId));
  auto rounded_frame = CreateResultsFrame(
      std::move(owned_omnibox_popup_webui_container_), location_bar_,
      /*forward_mouse_events=*/ShouldShowLocationBarCutout());
  rounded_frame->SetProperty(views::kElementIdentifierKey,
                             kRoundedResultsFrame);
  widget_->SetContentsView(std::move(rounded_frame));
  widget_->SetVisibilityChangedAnimationsEnabled(false);

  GetResultsFrame()->SetCutoutVisibility(ShouldShowLocationBarCutout());
}

std::unique_ptr<RoundedOmniboxResultsFrame>
OmniboxPopupPresenterBase::CreateResultsFrame(
    std::unique_ptr<views::View> contents,
    LocationBar* location_bar,
    bool forward_mouse_events) {
  return std::make_unique<RoundedOmniboxResultsFrame>(
      contents.release(), location_bar, forward_mouse_events);
}

bool OmniboxPopupPresenterBase::ShouldShowLocationBarCutout() const {
  return false;
}

bool OmniboxPopupPresenterBase::ShouldReceiveFocus() const {
  return true;
}

bool OmniboxPopupPresenterBase::ShouldHideForInitialLayout() const {
  return false;
}

bool OmniboxPopupPresenterBase::ShouldPreserveRequestedFocus() const {
  return false;
}

void OmniboxPopupPresenterBase::OnWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  is_deferred_ = false;
  // Drop stale visual state callbacks when the widget is closed.
  visual_state_weak_factory_.InvalidateWeakPtrs();
  if (auto* frame = GetResultsFrame()) {
    owned_omnibox_popup_webui_container_ = frame->ExtractContents();
  }
  // Call WidgetDestroyed() before resetting the widget pointer. This ensures
  // that subclasses can safely access the widget (e.g., to reset observations)
  // before it is destroyed, avoiding dangling pointer issues.
  WidgetDestroyed();
  widget_.reset();
}

void OmniboxPopupPresenterBase::ReleaseWidget() {
  if (widget_) {
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

RoundedOmniboxResultsFrame* OmniboxPopupPresenterBase::GetResultsFrame() const {
  CHECK(widget_);
  return views::AsViewClass<RoundedOmniboxResultsFrame>(
      widget_->GetContentsView());
}

// Avoid initialization order 'race conditions' by only interacting with WebUI
// controller once it is connected (which is when the web contents updates/is
// created).
void OmniboxPopupPresenterBase::PrimaryPageChanged(content::Page& page) {
  if (auto* content = GetWebUIContent()) {
    auto* wrapper = content->contents_wrapper();
    auto* webui_controller = wrapper ? wrapper->GetWebUIController() : nullptr;

    if (webui_controller) {
      webui_controller->SetPresenterDelegate(this);
    }
  }
}

void OmniboxPopupPresenterBase::OnEmbeddedPermissionDialogChanged(
    bool is_showing,
    const gfx::Size& prompt_size) {
  SetPermissionPromptShowing(is_showing);
  if (!is_showing) {
    // Set dismissal handling flag to ensure Omnibox popup does not close due to
    // `kBlur` while dismissal cleanup finishes running. (The prompt is not
    // showing during dismissal, so `is_showing` is false and does not prevent
    // Omnibox popup from closing via `SetPermissionPromptShowing`).
    // Do not call `FocusLocation()` here, as refocusing the omnibox on
    // PEPC dismissal re-triggers WebUI media access requests.
    is_handling_prompt_dismissal_ = true;
  }

  gfx::Size new_minimum_size = is_showing ? prompt_size : gfx::Size();

  if (minimum_size_ == new_minimum_size) {
    return;
  }

  minimum_size_ = new_minimum_size;

  // Use a PostTask to ensure the Mojo call stack is cleared
  // to avoid 'reentrancy' error since having 2 `OnLocationBarBoundsChanged`
  // on the call stack triggers that error.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<OmniboxPopupPresenterBase> presenter) {
            if (presenter && presenter->GetWebUIContent()) {
              presenter->GetWebUIContent()->OnLocationBarBoundsChanged();
            }
          },
          weak_factory_.GetWeakPtr()));
}

OmniboxController* OmniboxPopupPresenterBase::GetOmniboxController() {
  return controller();
}

OmniboxPopupPresenterBase::ScopedDeactivationBlocker::ScopedDeactivationBlocker(
    base::WeakPtr<OmniboxPopupPresenterBase> presenter)
    : presenter_(std::move(presenter)) {
  if (presenter_) {
    presenter_->RegisterBlocker();
  }
}

OmniboxPopupPresenterBase::ScopedDeactivationBlocker::
    ~ScopedDeactivationBlocker() {
  if (presenter_) {
    presenter_->UnregisterBlocker();
  }
}

std::unique_ptr<OmniboxPopupDeactivationBlocker>
OmniboxPopupPresenterBase::CreateDeactivationBlocker() {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxKeepOpenOnFileSelection)) {
    return nullptr;
  }
  return std::make_unique<ScopedDeactivationBlocker>(
      weak_factory_.GetWeakPtr());
}

void OmniboxPopupPresenterBase::RegisterBlocker() {
  deactivation_blockers_count_++;
}

void OmniboxPopupPresenterBase::UnregisterBlocker() {
  deactivation_blockers_count_--;

  DCHECK_GE(deactivation_blockers_count_, 0);
  if (deactivation_blockers_count_ < 0) {
    deactivation_blockers_count_ = 0;
  }

  if (deactivation_blockers_count_ == 0 && location_bar() && IsShown()) {
    RequestFocus();
  }
}

void OmniboxPopupPresenterBase::OnFileSelectionClosed() {}

void OmniboxPopupPresenterBase::SetPermissionPromptShowing(bool showing) {
  is_prompt_showing_ = showing;
}

void OmniboxPopupPresenterBase::ResetPermissionPromptShowingState() {
  is_prompt_showing_ = false;
  is_handling_prompt_dismissal_ = false;
}

void OmniboxPopupPresenterBase::HandlePermissionPromptDismissal() {
  SetPermissionPromptShowing(false);
  is_handling_prompt_dismissal_ = true;
  if (location_bar()) {
    location_bar()->FocusLocation(/*is_user_initiated=*/false,
                                  /*clear_focus_if_failed=*/false);
  }
}

void OmniboxPopupPresenterBase::OnPromptAdded() {
  SetPermissionPromptShowing(true);
}

void OmniboxPopupPresenterBase::OnPromptRemoved() {
  HandlePermissionPromptDismissal();
}

void OmniboxPopupPresenterBase::OnPromptRecreateViewFailed() {
  SetPermissionPromptShowing(false);
}

void OmniboxPopupPresenterBase::OnPromptCreationFailedHiddenTab() {
  SetPermissionPromptShowing(false);
}

void OmniboxPopupPresenterBase::OnRequestsFinalized() {
  SetPermissionPromptShowing(false);
}

void OmniboxPopupPresenterBase::OnPermissionRequestManagerDestructed() {
  permission_observation_.Reset();
  is_prompt_showing_ = false;
  is_handling_prompt_dismissal_ = false;
}

void OmniboxPopupPresenterBase::OnWidgetActivated() {
  is_handling_prompt_dismissal_ = false;
}

bool OmniboxPopupPresenterBase::IsPermissionPromptPreventingClose() const {
  return is_prompt_showing_ || is_handling_prompt_dismissal_;
}
