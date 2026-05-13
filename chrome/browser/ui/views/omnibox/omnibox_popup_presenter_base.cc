// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
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

  has_logged_content_ready_since_open_ = false;

  EnsureWidgetCreated();
  SynchronizePopupBounds();

  if (auto* content = GetWebUIContent()) {
    content->ShowUI();

    // TODO(crbug.com/507159575): Refactor into `OnVisualStateReady` callback to
    // avoid registering a 2nd callback when the classic popup is deferred.
    // Log result ready metric before checking deferral logic. This ensures we
    // don't miss the initial frame commit if we don't defer.
    LogResultToContentReadyMetric(content->GetWebContents());

    auto show_request_time = base::TimeTicks::Now();
    auto timeout = ShouldDeferUntilVisualStateReady();
    if (timeout.has_value()) {
      is_deferred_ = true;

      // Call WasShown to mark the WebContents as visible so that a frame will
      // eventually be produced that triggers the OnVisualStateReady callback.
      content->GetWebContents()->WasShown();

      content->GetWebContents()
          ->GetPrimaryMainFrame()
          ->InsertVisualStateCallback(
              base::BindOnce(&OmniboxPopupPresenterBase::OnVisualStateReady,
                             weak_factory_.GetWeakPtr(), show_request_time,
                             /*from_fallback=*/false));

      // Add a backup timer in case the visual state callback is never called.
      // The visual state callback should always be called, but this fallback
      // ensures that if that assumption is ever broken, the UI will eventually
      // be shown.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&OmniboxPopupPresenterBase::OnVisualStateReady,
                         weak_factory_.GetWeakPtr(), show_request_time,
                         /*from_fallback=*/true,
                         /*success=*/false),
          timeout.value());
    } else {
      ShowWidget(show_request_time);
    }
  }
}

void OmniboxPopupPresenterBase::OnVisualStateReady(
    base::TimeTicks show_request_time,
    bool from_fallback,
    bool success) {
  if (!is_deferred_) {
    return;
  }

  base::UmaHistogramBoolean(
      base::StrCat(
          {GetPopupMetricPrefix(), ".DeferredShowVisualStateReadyFromTimeout"}),
      from_fallback);

  base::TimeDelta duration = base::TimeTicks::Now() - show_request_time;
  base::UmaHistogramTimes(
      base::StrCat(
          {GetPopupMetricPrefix(), ".DeferredShowVisualStateReadyDuration"}),
      duration);

  is_deferred_ = false;
  // Fall back to showing the widget even if success == false
  // so the UI state matches the requested visibility.
  ShowWidget(show_request_time);
}

void OmniboxPopupPresenterBase::ShowWidget(base::TimeTicks show_request_time) {
  widget_->ShowInactive();
  widget_->GetCompositor()->RequestPresentationTimeForNextFrame(base::BindOnce(
      [](std::string uma_metric, base::TimeTicks show_request_time,
         const gfx::PresentationFeedback& feedback) {
        // If there is ever an error, the timestamp means the timestamp
        // of the error. In that case we shouldn't record anything.
        if (feedback.failed()) {
          return;
        }
        const base::TimeDelta delta = feedback.timestamp - show_request_time;
        base::UmaHistogramTimes(uma_metric, delta);
      },
      base::StrCat({GetPopupMetricPrefix(), ".PresenterShowLatency.ToPaint"}),
      show_request_time));

  if (auto* content = GetWebUIContent()) {
    content->GetWebContents()->WasShown();
    if (ShouldReceiveFocus()) {
      widget_->Activate();
      content->RequestFocus();
      content->GetWebContents()->Focus();
    }
  }
}

void OmniboxPopupPresenterBase::LogResultToContentReadyMetric(
    content::WebContents* web_contents) {
  if (GetPopupMetricPrefix() != kWebUIPopupMetricPrefix) {
    // TODO(crbug.com/491337216): Measure this for the AIM popup as well, with a
    // consistent metric prefix for both popup types.
    // Skipping AIM popups for now to maintain parity with the Views popups.
    return;
  }

  web_contents->GetPrimaryMainFrame()->InsertVisualStateCallback(base::BindOnce(
      &OmniboxPopupPresenterBase::OnVisualStateReadyForMetrics,
      weak_factory_.GetWeakPtr(),
      controller()->autocomplete_controller()->result().result_ready_time()));
}

void OmniboxPopupPresenterBase::OnVisualStateReadyForMetrics(
    base::TimeTicks result_ready_time,
    bool success) {
  if (result_ready_time.is_null()) {
    omnibox::LogResultToContentReadyEarlyExitReason(
        omnibox::ResultToContentReadyEarlyExitReason::kNoResultReadyTime);
    return;
  }

  if (!success) {
    omnibox::LogResultToContentReadyEarlyExitReason(
        omnibox::ResultToContentReadyEarlyExitReason::kVisualStateNotReady);
    return;
  }

  const base::TimeDelta delta = base::TimeTicks::Now() - result_ready_time;

  if (!has_logged_content_ready_since_open_) {
    base::UmaHistogramTimes("Omnibox.Popup.ResultToContentReadyPerShow", delta);
    has_logged_content_ready_since_open_ = true;
  }

  if (!has_logged_first_content_ready_) {
    base::UmaHistogramTimes("Omnibox.Popup.ResultToContentReadyOnFirstShow",
                            delta);
    has_logged_first_content_ready_ = true;
  }
}

void OmniboxPopupPresenterBase::Hide() {
  is_deferred_ = false;
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
  SynchronizePopupBounds();
}

void OmniboxPopupPresenterBase::SynchronizePopupBounds() {
  if (widget_) {
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
  auto rounded_frame = std::make_unique<RoundedOmniboxResultsFrame>(
      owned_omnibox_popup_webui_container_.release(), location_bar_,
      /*forward_mouse_events=*/ShouldShowLocationBarCutout());
  rounded_frame->SetProperty(views::kElementIdentifierKey,
                             kRoundedResultsFrame);
  widget_->SetContentsView(std::move(rounded_frame));
  widget_->SetVisibilityChangedAnimationsEnabled(false);

  GetResultsFrame()->SetCutoutVisibility(ShouldShowLocationBarCutout());
}

bool OmniboxPopupPresenterBase::ShouldShowLocationBarCutout() const {
  return false;
}

bool OmniboxPopupPresenterBase::ShouldReceiveFocus() const {
  return true;
}

void OmniboxPopupPresenterBase::OnWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  is_deferred_ = false;
  owned_omnibox_popup_webui_container_ = GetResultsFrame()->ExtractContents();
  widget_.reset();
  WidgetDestroyed();
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

OmniboxController* OmniboxPopupPresenterBase::controller() const {
  return controller_;
}
