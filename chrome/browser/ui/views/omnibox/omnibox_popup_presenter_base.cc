// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_client.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/native_widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/omnibox/omnibox_popup_native_widget_mac.h"
#endif

namespace omnibox {
const void* kOmniboxWebUIPopupWidgetId = &kOmniboxWebUIPopupWidgetId;
}

namespace {

std::string_view CreationReasonToString(
    OmniboxPopupPresenterBase::CreationReason reason) {
  switch (reason) {
    case OmniboxPopupPresenterBase::CreationReason::kEager:
      return "Eager";
    case OmniboxPopupPresenterBase::CreationReason::kShow:
      return "Show";
    case OmniboxPopupPresenterBase::CreationReason::kContextMenu:
      return "ContextMenu";
    case OmniboxPopupPresenterBase::CreationReason::kOmniboxFocus:
      return "OmniboxFocus";
    case OmniboxPopupPresenterBase::CreationReason::kAfterWindowPresentation:
      return "AfterWindowPresentation";
  }
}

// Whether the content is created to prewarm the popup, before anything needs
// it.
bool IsPrewarm(OmniboxPopupPresenterBase::CreationReason reason) {
  switch (reason) {
    case OmniboxPopupPresenterBase::CreationReason::kEager:
    case OmniboxPopupPresenterBase::CreationReason::kAfterWindowPresentation:
    case OmniboxPopupPresenterBase::CreationReason::kOmniboxFocus:
      return true;
    case OmniboxPopupPresenterBase::CreationReason::kShow:
    case OmniboxPopupPresenterBase::CreationReason::kContextMenu:
      return false;
  }
}

}  // namespace

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
  pool_client_.reset();
  ReleaseWidget();
  // `ReleaseWidget()` only queues the widget for deferred destruction, so drain
  // the queue here: this destroys both the widget just closed above and any
  // left over from earlier `Hide()`s. Teardown is therefore still synchronous
  // for the destructor, and nothing may outlive `this`.
  widgets_pending_deletion_.clear();
}

void OmniboxPopupPresenterBase::Show() {
  GetOrCreateWebUIContent(CreationReason::kShow);
  if (IsShown()) {
    // If a re-entrant `Show()` occurs while the widget is still visible but
    // WebContents was set to hidden by a preceding `Hide()`, ensure it is woken
    // up.
    if (auto* content = GetWebUIContent()) {
      if (auto* web_contents = content->GetWebContents();
          web_contents &&
          web_contents->GetVisibility() == content::Visibility::HIDDEN) {
        web_contents->WasShown();
      }
    }
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
    is_deferred_ = timeout.has_value();
    content->GetWebContents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
        base::BindOnce(&OmniboxPopupPresenterBase::OnVisualStateReady,
                       visual_state_weak_factory_.GetWeakPtr(),
                       show_request_time, result_ready_time,
                       /*from_fallback=*/false));
    if (is_deferred_) {
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
  is_open_ = true;
  OnWidgetPresentedChanged(false);
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
  // Report even on failure so that nothing waits forever.
  OnWidgetPresentedChanged(true);
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
  TRACE_EVENT("omnibox", "OmniboxPopupPresenterBase::Hide");
  permission_observation_.Reset();
  is_prompt_showing_ = false;
  is_handling_prompt_dismissal_ = false;

  if (ShouldPreserveRequestedFocus()) {
    focus_requested_ = false;
  }
  is_deferred_ = false;
  // Drop stale visual state callbacks.
  visual_state_weak_factory_.InvalidateWeakPtrs();
  OnWidgetPresentedChanged(false);

  // Only close if UI DevTools settings allow.
  if (widget_ && widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
    // If external code already hid the widget (e.g. ash on a user switch), then
    // we need to destroy the widget to prevent the external code from
    // re-showing the widget.
    const bool was_open = is_open_;
    const bool hidden_externally = was_open && !widget_->IsVisible();
    is_open_ = false;
    widget_->Hide();
    if (auto* content = GetWebUIContent()) {
      if (base::FeatureList::IsEnabled(
              omnibox::kOmniboxWebUIPopupMarkAsHidden)) {
        if (content->GetWebContents()) {
          content->GetWebContents()->WasHidden();
        }
      }
      content->Clear();
    }

    // Discard the widget so the next show gets a brand new native window that
    // has never presented a frame, instead of one that may still hold content
    // from this show. `OnWidgetClosed()` extracts the WebUI container before
    // the widget is destroyed, so the WebContents (and the pre-warmed
    // renderer) survives and is re-parented by `EnsureWidgetCreated()`.
    if (ShouldDestroyWidgetOnHide() || hidden_externally) {
      // Flag the teardown as deliberate so `WidgetDestroyed()` overrides don't
      // mistake it for the widget being closed out from under us (e.g. by the
      // OS) and re-enter the popup state teardown already in progress.
      base::AutoReset<bool> destroying(&is_destroying_widget_, true);
      ReleaseWidget();
    }

    if (was_open && pool_client_) {
      pool_client_->OnPopupHidden();
    }
  }
}

bool OmniboxPopupPresenterBase::IsShown() const {
  return is_deferred_ || (widget_ && widget_->IsVisible());
}

void OmniboxPopupPresenterBase::OnContentHeightChanged(int content_height) {
  if (ShouldDrawShadowInWebUI()) {
    // The WebUI reserves the shadow margin inside its own document, so the
    // reported height includes it. Strip it back out so `content_height_`
    // keeps meaning "height of the popup content"; `SynchronizePopupBounds()`
    // adds the shadow margin back when sizing the widget.
    // TODO(crbug.com/563367122): Streamline height coordination between WebUI
    // and Views so shadow insets do not need to be manually stripped and
    // re-added.
    content_height =
        std::max(1, content_height -
                        RoundedOmniboxResultsFrame::GetShadowInsets().height());
  }
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

OmniboxPopupWebUIBaseContent* OmniboxPopupPresenterBase::GetWebUIContent() {
  return const_cast<OmniboxPopupWebUIBaseContent*>(
      std::as_const(*this).GetWebUIContent());
}

const OmniboxPopupWebUIBaseContent* OmniboxPopupPresenterBase::GetWebUIContent()
    const {
  return omnibox_popup_webui_content_;
}

void OmniboxPopupPresenterBase::SetWebUIContent(
    std::unique_ptr<OmniboxPopupWebUIBaseContent> webui_content) {
  omnibox_popup_webui_content_ =
      GetUIContainer()->AddChildView(std::move(webui_content));

  Observe(omnibox_popup_webui_content_->GetWebContents());

  // A hidden popup needs no native window, and building one per popup slows
  // down browser window creation. `Show()` creates it on demand.
  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIPopupCreateWidgetOnShow)) {
    return;
  }
  EnsureWidgetCreated();

  // Explicitly put the presenter into its resting hidden state upon creation.
  // This ensures the pre-warmed widget and WebContents are marked hidden and
  // detached from the GPU compositor until `Show()` is called.
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxWebUIPopupHideOnCreation)) {
    Hide();
  }
}

OmniboxPopupWebUIBaseContent*
OmniboxPopupPresenterBase::GetOrCreateWebUIContent(CreationReason reason) {
  if (auto* content = GetWebUIContent()) {
    return content;
  }
  if (pool_client_) {
    if (IsPrewarm(reason)) {
      if (pool_client_->HasSharedWebUIContent()) {
        return nullptr;
      }
    } else if (auto contents = pool_client_->TakeIdleWebUIContents()) {
      return AdoptWebUIContents(std::move(contents));
    }
  }
  // Covers `CreateWebUIContent()`, which creates the WebContents and starts
  // its navigation.
  // TODO(crbug.com/570224989): Record an enum histogram of `CreationReason` for
  // presenters using the deferred creation strategy.
  TRACE_EVENT("omnibox", "OmniboxPopupPresenterBase::GetOrCreateWebUIContent",
              "prefix", GetPopupMetricPrefix(), "reason",
              CreationReasonToString(reason));
  std::unique_ptr<OmniboxPopupWebUIBaseContent> content =
      CreateWebUIContent(/*contents=*/nullptr);
  if (!content) {
    // Every production presenter either sets its content in its constructor
    // or overrides `CreateWebUIContent()`. Only unit-test presenters without
    // WebUI content get here.
    CHECK_IS_TEST();
    return nullptr;
  }
  SetWebUIContent(std::move(content));
  return GetWebUIContent();
}

std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
OmniboxPopupPresenterBase::ReleaseWebUIContents() {
  OmniboxPopupWebUIBaseContent* content = GetWebUIContent();
  CHECK(content);
  CHECK(!IsShown());
  TRACE_EVENT("omnibox", "OmniboxPopupPresenterBase::ReleaseWebUIContents",
              "prefix", GetPopupMetricPrefix());
  Observe(nullptr);
  permission_observation_.Reset();
  std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents =
      content->ReleaseContentsWrapper();
  omnibox_popup_webui_content_ = nullptr;
  content->parent()->RemoveChildViewT(content);
  return contents;
}

bool OmniboxPopupPresenterBase::CanReleaseWebUIContents() {
  OmniboxPopupWebUIBaseContent* content = GetWebUIContent();
  return content && !IsShown() && !is_open_ && !has_active_blockers() &&
         !IsPermissionPromptPreventingClose() &&
         content->CanReleaseContentsWrapper();
}

OmniboxPopupState OmniboxPopupPresenterBase::GetSharedPopupKind() const {
  return OmniboxPopupState::kNone;
}

OmniboxPopupWebUIBaseContent* OmniboxPopupPresenterBase::AdoptWebUIContents(
    std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents) {
  CHECK(contents);
  CHECK(!GetWebUIContent());
  TRACE_EVENT("omnibox", "OmniboxPopupPresenterBase::AdoptWebUIContents",
              "prefix", GetPopupMetricPrefix());
  std::unique_ptr<OmniboxPopupWebUIBaseContent> content =
      CreateWebUIContent(std::move(contents));
  CHECK(content);
  SetWebUIContent(std::move(content));
  return GetWebUIContent();
}

// static
OmniboxPopupPresenterBase::WebUIContentCreationStrategy
OmniboxPopupPresenterBase::GetWebUIContentCreationStrategy(
    const base::Feature& defer_creation_feature) {
  if (omnibox::ShouldCreatePopupWebUIOnDemand()) {
    return WebUIContentCreationStrategy::kOnDemand;
  }
  if (base::FeatureList::IsEnabled(defer_creation_feature)) {
    return WebUIContentCreationStrategy::kAfterWindowPresentation;
  }
  return WebUIContentCreationStrategy::kEager;
}

OmniboxPopupPresenterBase::WebUIContentCreationStrategy
OmniboxPopupPresenterBase::GetDeferredWebUIContentCreationStrategy() const {
  return WebUIContentCreationStrategy::kAfterWindowPresentation;
}

void OmniboxPopupPresenterBase::ApplyWebUIContentCreationStrategy(
    const base::Feature& defer_creation_feature) {
  WebUIContentCreationStrategy strategy =
      GetWebUIContentCreationStrategy(defer_creation_feature);
  if (strategy == WebUIContentCreationStrategy::kAfterWindowPresentation) {
    strategy = GetDeferredWebUIContentCreationStrategy();
  }
  ApplyWebUIContentCreationStrategy(strategy);
}

void OmniboxPopupPresenterBase::ApplyWebUIContentCreationStrategy(
    WebUIContentCreationStrategy strategy) {
  content_creation_strategy_ = strategy;
  // Joins the profile's pool before prewarming, so that prewarming can see the
  // other windows' popups. `location_bar_` is null in unit tests.
  if (location_bar_) {
    views::Widget* widget = presenter_delegate_->GetLocationBarWidget();
    pool_client_ = OmniboxPopupWebUIPoolClient::CreateIfSharingEnabled(
        *this, location_bar_->GetProfile(),
        widget ? widget->GetTopLevelWidget() : nullptr);
  }
  switch (strategy) {
    case WebUIContentCreationStrategy::kEager:
      GetOrCreateWebUIContent(CreationReason::kEager);
      return;
    case WebUIContentCreationStrategy::kAfterWindowPresentation:
    case WebUIContentCreationStrategy::kOnOmniboxFocus:
      CreateWebUIContentAfterWindowPresentation();
      return;
    case WebUIContentCreationStrategy::kOnDemand:
      return;
  }
}

std::unique_ptr<OmniboxPopupWebUIBaseContent>
OmniboxPopupPresenterBase::CreateWebUIContent(
    std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>> contents) {
  return nullptr;
}

void OmniboxPopupPresenterBase::CreateWebUIContentAfterWindowPresentation() {
  views::Widget* widget = delegate().GetLocationBarWidget();
  // Key on the browser window's widget rather than the macOS immersive
  // fullscreen overlay that may host the location bar.
  views::Widget* browser_widget =
      widget ? widget->GetTopLevelWidget() : nullptr;
  if (!browser_widget) {
    // `Show()` and the other callers of `GetOrCreateWebUIContent()` still
    // create the content when they need it.
    return;
  }
  browser_widget_observation_.Observe(browser_widget);
  UpdateBrowserWidgetPresentationRequest();
}

void OmniboxPopupPresenterBase::OnBrowserWindowPresented() {
  has_browser_window_presented_ = true;
  // With `kOnOmniboxFocus`, wait for the omnibox to be focused. If it was
  // focused before this first frame, e.g. in a new window opened on the New Tab
  // page, `OnOmniboxFocused()` skipped creation so as not to delay the window,
  // and the content is created here instead.
  if (content_creation_strategy_ ==
          WebUIContentCreationStrategy::kOnOmniboxFocus &&
      !omnibox_focused_) {
    return;
  }
  if (deferred_creation_scheduled_ || GetWebUIContent()) {
    return;
  }
  deferred_creation_scheduled_ = true;
  CreationReason reason = content_creation_strategy_ ==
                                  WebUIContentCreationStrategy::kOnOmniboxFocus
                              ? CreationReason::kOmniboxFocus
                              : CreationReason::kAfterWindowPresentation;
  // Post rather than create inline, so that the WebContents is not created
  // inside the presentation feedback callback.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &OmniboxPopupPresenterBase::GetOrCreateWebUIContent),
                     weak_factory_.GetWeakPtr(), reason));
}

void OmniboxPopupPresenterBase::OnOmniboxFocused() {
  omnibox_focused_ = true;
  // A window that opens with its omnibox focused gets here before its first
  // frame. Creating the content then would put the popup's WebContents back on
  // the window's critical path, so `OnBrowserWindowPresented()` creates it.
  if (content_creation_strategy_ !=
          WebUIContentCreationStrategy::kOnOmniboxFocus ||
      !has_browser_window_presented_) {
    return;
  }
  GetOrCreateWebUIContent(CreationReason::kOmniboxFocus);
}

OmniboxPopupPresenterBase::BrowserWidgetObserver::BrowserWidgetObserver(
    OmniboxPopupPresenterBase& presenter)
    : presenter_(presenter) {}

OmniboxPopupPresenterBase::BrowserWidgetObserver::~BrowserWidgetObserver() =
    default;

void OmniboxPopupPresenterBase::BrowserWidgetObserver::OnWidgetDestroying(
    views::Widget* widget) {
  presenter_->StopObservingBrowserWidget();
}

void OmniboxPopupPresenterBase::BrowserWidgetObserver::
    OnWidgetVisibilityChanged(views::Widget* widget, bool visible) {
  presenter_->UpdateBrowserWidgetPresentationRequest();
}

void OmniboxPopupPresenterBase::BrowserWidgetObserver::OnWidgetShowStateChanged(
    views::Widget* widget) {
  presenter_->UpdateBrowserWidgetPresentationRequest();
}

void OmniboxPopupPresenterBase::UpdateBrowserWidgetPresentationRequest() {
  views::Widget* widget = browser_widget_observation_.GetSource();
  // On ChromeOS Ash all windows on a display share one `ui::Compositor`, so a
  // callback requested while this window is hidden or minimized would run when
  // another window presents a frame. `Widget::IsVisible()` also checks ancestor
  // visibility, so a window shown on an inactive Ash desk stays false here.
  // Activating the desk doesn't notify this widget, so such a window creates
  // its popup content on demand unless it is later hidden or minimized and
  // shown again.
  // TODO(crbug.com/570224989): On Aura, also observe the native window, whose
  // visibility changes propagate from its ancestors, to re-arm on desk
  // activation.
  if (!widget || !widget->IsVisible() || widget->IsMinimized()) {
    browser_presentation_weak_factory_.InvalidateWeakPtrs();
    return;
  }
  // A live weak pointer means a request is outstanding. If the compositor
  // drops it unrun (e.g. the display is removed or the window moves to another
  // display), the next visibility change requests another.
  ui::Compositor* compositor = widget->GetCompositor();
  if (browser_presentation_weak_factory_.HasWeakPtrs() || !compositor) {
    return;
  }
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      base::IgnoreArgs<const viz::FrameTimingDetails&>(
          base::BindOnce(&OmniboxPopupPresenterBase::OnBrowserWidgetPresented,
                         browser_presentation_weak_factory_.GetWeakPtr())));
  // Views does not record whether a widget has already presented, so an idle
  // compositor presents no frame for a presenter built later (e.g. when AI Mode
  // eligibility arrives). A window still being built paints this as part of its
  // first frame.
  widget->GetRootView()->SchedulePaintInRect(gfx::Rect(1, 1));
}

void OmniboxPopupPresenterBase::OnBrowserWidgetPresented() {
  StopObservingBrowserWidget();
  OnBrowserWindowPresented();
}

void OmniboxPopupPresenterBase::StopObservingBrowserWidget() {
  browser_widget_observation_.Reset();
  browser_presentation_weak_factory_.InvalidateWeakPtrs();
}

bool OmniboxPopupPresenterBase::ShouldDestroyWidgetOnHide() const {
  return delegate().is_full_webui_omnibox() &&
         base::FeatureList::IsEnabled(
             omnibox::kOmniboxFullWebUIDestroyWidgetOnHide);
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
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent_widget->GetNativeView();
  params.context = parent_widget->GetNativeWindow();

  RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_.get());
#if BUILDFLAG(IS_MAC)
  // Makes window managers move the browser window, not the popup.
  if (delegate().is_full_webui_omnibox()) {
    params.native_widget = CreateOmniboxPopupNativeWidget(widget_.get());
  }
#endif

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

bool OmniboxPopupPresenterBase::ShouldDrawShadowInWebUI() const {
  return false;
}

bool OmniboxPopupPresenterBase::CompletedFirstVisuallyNonEmptyPaint() {
  if (completed_first_visually_non_empty_paint_for_testing_.has_value()) {
    return *completed_first_visually_non_empty_paint_for_testing_;
  }
  // The popup keeps its page across shows, so this stays true once the page
  // first paints. Read the wrapped contents, since `GetWebContents()` creates
  // new contents while the popup is hidden and its contents are detached.
  OmniboxPopupWebUIBaseContent* content = GetWebUIContent();
  content::WebContents* web_contents =
      content ? content->GetWrappedWebContents() : nullptr;
  return web_contents && web_contents->CompletedFirstVisuallyNonEmptyPaint();
}

void OmniboxPopupPresenterBase::
    SetCompletedFirstVisuallyNonEmptyPaintForTesting(bool completed) {
  completed_first_visually_non_empty_paint_for_testing_ = completed;
  delegate().OnPopupFirstVisuallyNonEmptyPaint();
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
  is_open_ = false;
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

  // Clearing `widget_` here is what makes the next `Show()` build a fresh
  // native window, but the object itself must outlive the current call stack.
  // Every close defers, because the one that cannot be synchronous is not
  // distinguishable here: `Hide()` runs from inside
  // `WidgetObserver::OnWidgetActivationChanged()` on the click-outside path,
  // and `Widget::OnNativeWidgetActivationChanged()` still touches `this` after
  // notifying its observers. Retain ownership rather than using `DeleteSoon()`
  // so the widget is still destroyed if the message loop stops before the task
  // runs, as happens during shutdown; the destructor drains the queue inline
  // for the same reason.
  //
  // Append rather than overwrite: a `Hide()`/`Show()`/`Hide()` sequence within
  // a single task would otherwise drop the previous widget here, destroying it
  // synchronously from inside this one's teardown.
  widgets_pending_deletion_.push_back(std::move(widget_));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OmniboxPopupPresenterBase::DeletePendingWidgets,
                     weak_factory_.GetWeakPtr()));
}

void OmniboxPopupPresenterBase::DeletePendingWidgets() {
  // Destroy from a local so that anything re-entrant triggered by widget
  // destruction sees an empty queue and can safely append to it.
  std::vector<std::unique_ptr<views::Widget>> widgets;
  widgets.swap(widgets_pending_deletion_);
}

void OmniboxPopupPresenterBase::ReleaseWidget() {
  if (widget_) {
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

RoundedOmniboxResultsFrame* OmniboxPopupPresenterBase::GetResultsFrame() const {
  // The widget may be absent while hidden; see `ShouldDestroyWidgetOnHide()`.
  if (!widget_) {
    return nullptr;
  }
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

  CHECK_GE(deactivation_blockers_count_, 0, base::NotFatalUntil::M162);
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
  // If the prompt was removed because the popup widget itself was hidden (e.g.
  // during browser window teardown), do not enter dismissal mode or refocus the
  // location bar.
  if (!IsShown()) {
    return;
  }
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
