// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search_bubble_host.h"

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabSearchOpenAction {
  kMouseClick = 0,
  kKeyboardNavigation = 1,
  kKeyboardShortcut = 2,
  kTouchGesture = 3,
  kMaxValue = kTouchGesture,
};

TabSearchOpenAction GetActionForEvent(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    return TabSearchOpenAction::kMouseClick;
  }
  return event.IsKeyEvent() ? TabSearchOpenAction::kKeyboardNavigation
                            : TabSearchOpenAction::kTouchGesture;
}

}  // namespace

TabSearchBubbleHost::TabSearchBubbleHost(views::Button* button,
                                         Profile* profile)
    : button_(button),
      profile_(profile),
      webui_bubble_manager_(button,
                            profile,
                            GURL(chrome::kChromeUITabSearchURL),
                            IDS_ACCNAME_TAB_SEARCH),
      widget_open_timer_(base::BindRepeating([](base::TimeDelta time_elapsed) {
        base::UmaHistogramMediumTimes("Tabs.TabSearch.WindowDisplayedDuration3",
                                      time_elapsed);
      })) {
  auto menu_button_controller = std::make_unique<views::MenuButtonController>(
      button,
      base::BindRepeating(&TabSearchBubbleHost::ButtonPressed,
                          base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(button));
  menu_button_controller_ = menu_button_controller.get();
  button->SetButtonController(std::move(menu_button_controller));
}

TabSearchBubbleHost::~TabSearchBubbleHost() = default;

void TabSearchBubbleHost::OnWidgetVisibilityChanged(views::Widget* widget,
                                                    bool visible) {
  DCHECK_EQ(webui_bubble_manager_.GetBubbleWidget(), widget);
  if (visible && bubble_created_time_.has_value()) {
    button_->GetWidget()
        ->GetCompositor()
        ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
            [](base::TimeTicks bubble_created_time,
               bool bubble_using_cached_web_contents,
               base::TimeTicks presentation_timestamp) {
              base::UmaHistogramMediumTimes(
                  bubble_using_cached_web_contents
                      ? "Tabs.TabSearch.WindowTimeToShowCachedWebView"
                      : "Tabs.TabSearch.WindowTimeToShowUncachedWebView",
                  presentation_timestamp - bubble_created_time);
            },
            *bubble_created_time_,
            webui_bubble_manager_.bubble_using_cached_web_contents()));
    bubble_created_time_.reset();
  }
}

void TabSearchBubbleHost::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(webui_bubble_manager_.GetBubbleWidget(), widget);
  DCHECK(bubble_widget_observation_.IsObservingSource(
      webui_bubble_manager_.GetBubbleWidget()));
  bubble_widget_observation_.Reset();
  pressed_lock_.reset();
}

bool TabSearchBubbleHost::ShowTabSearchBubble(
    bool triggered_by_keyboard_shortcut) {
  if (webui_bubble_manager_.GetBubbleWidget())
    return false;

  // Close the Tab Search IPH if it is showing.
  BrowserFeaturePromoController* controller =
      BrowserFeaturePromoController::GetForView(button_);
  if (controller)
    controller->EndPromo(
        feature_engagement::kIPHTabSearchFeature,
        user_education::FeaturePromoCloseReason::kFeatureEngaged);

  absl::optional<gfx::Rect> anchor;
  if (button_->GetWidget()->IsFullscreen() && !button_->IsDrawn()) {
    // Use a screen-coordinate anchor rect when the tabstrip's search button is
    // not drawn, and potentially positioned offscreen, in fullscreen mode.
    // Place the anchor similar to where the button would be in non-fullscreen
    // mode.
    gfx::Rect bounds = button_->GetWidget()->GetWorkAreaBoundsInScreen();
    int offset = GetLayoutConstant(TABSTRIP_REGION_VIEW_CONTROL_PADDING);

    int x = ShouldTabSearchRenderBeforeTabStrip() ? bounds.x() + offset
                                                  : bounds.right() - offset;

    anchor.emplace(gfx::Rect(x, bounds.y() + offset, 0, 0));
  }

  bubble_created_time_ = base::TimeTicks::Now();
  webui_bubble_manager_.ShowBubble(anchor,
                                   ShouldTabSearchRenderBeforeTabStrip()
                                       ? views::BubbleBorder::TOP_LEFT
                                       : views::BubbleBorder::TOP_RIGHT,
                                   kTabSearchBubbleElementId);

  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  if (tracker)
    tracker->NotifyEvent(feature_engagement::events::kTabSearchOpened);

  if (triggered_by_keyboard_shortcut) {
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  TabSearchOpenAction::kKeyboardShortcut);
  }

  // There should only ever be a single bubble widget active for the
  // TabSearchBubbleHost.
  DCHECK(!bubble_widget_observation_.IsObserving());
  bubble_widget_observation_.Observe(webui_bubble_manager_.GetBubbleWidget());
  widget_open_timer_.Reset(webui_bubble_manager_.GetBubbleWidget());

  // Hold the pressed lock while the |bubble_| is active.
  pressed_lock_ = menu_button_controller_->TakeLock();
  return true;
}

void TabSearchBubbleHost::CloseTabSearchBubble() {
  webui_bubble_manager_.CloseBubble();
}

void TabSearchBubbleHost::ButtonPressed(const ui::Event& event) {
  if (ShowTabSearchBubble()) {
    // Only log the open action if it resulted in creating a new instance of the
    // Tab Search bubble.
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  GetActionForEvent(event));

    webui_bubble_manager_.GetBubbleWidget()
        ->GetCompositor()
        ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
            [](base::TimeTicks button_pressed_time,
               base::TimeTicks presentation_timestamp) {
              base::UmaHistogramMediumTimes(
                  "Tabs.TabSearch."
                  "ButtonPressedToNextFramePresented",
                  presentation_timestamp - button_pressed_time);
            },
            base::TimeTicks::Now()));
    return;
  }
  CloseTabSearchBubble();
}

bool TabSearchBubbleHost::ShouldTabSearchRenderBeforeTabStrip() {
// Mac should have tabsearch on the right side. Windows >= Win10 has the
// Tab Search button as a FrameCaptionButton, but it still needs to be on the
// left if it exists.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return features::IsChromeRefresh2023();
#else
  return false;
#endif
}
