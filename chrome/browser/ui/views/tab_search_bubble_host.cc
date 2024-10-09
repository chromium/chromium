// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search_bubble_host.h"

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
      webui_bubble_manager_(WebUIBubbleManager::Create<TabSearchUI>(
          button,
          profile,
          GURL(chrome::kChromeUITabSearchURL),
          IDS_ACCNAME_TAB_SEARCH)),
      widget_open_timer_(base::BindRepeating([](base::TimeDelta time_elapsed) {
        base::UmaHistogramMediumTimes("Tabs.TabSearch.WindowDisplayedDuration3",
                                      time_elapsed);
      })) {
  auto* const tab_organization_service =
      TabOrganizationServiceFactory::GetForProfile(profile);
  if (tab_organization_service) {
    tab_organization_observation_.Observe(tab_organization_service);
  }
  auto menu_button_controller = std::make_unique<views::MenuButtonController>(
      button,
      base::BindRepeating(&TabSearchBubbleHost::ButtonPressed,
                          base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(button));
  menu_button_controller_ = menu_button_controller.get();
  button->SetButtonController(std::move(menu_button_controller));
  webui_bubble_manager_observer_.Observe(webui_bubble_manager_.get());
}

TabSearchBubbleHost::~TabSearchBubbleHost() = default;

void TabSearchBubbleHost::OnWidgetVisibilityChanged(views::Widget* widget,
                                                    bool visible) {
  CHECK_EQ(webui_bubble_manager_->GetBubbleWidget(), widget);
  if (visible && widget && bubble_created_time_.has_value()) {
    widget->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
        base::BindOnce(
            [](base::TimeTicks bubble_created_time,
               bool bubble_using_cached_web_contents,
               WebUIContentsWarmupLevel contents_warmup_level,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeTicks presentation_timestamp =
                  frame_timing_details.presentation_feedback.timestamp;
              base::TimeDelta time_to_show =
                  presentation_timestamp - bubble_created_time;
              base::UmaHistogramMediumTimes(
                  bubble_using_cached_web_contents
                      ? "Tabs.TabSearch.WindowTimeToShowCachedWebView2"
                      : "Tabs.TabSearch.WindowTimeToShowUncachedWebView2",
                  time_to_show);
              base::UmaHistogramMediumTimes(
                  base::StrCat({"Tabs.TabSearch.TimeToShow.",
                                ToString(contents_warmup_level)}),
                  time_to_show);
            },
            *bubble_created_time_,
            webui_bubble_manager_->bubble_using_cached_web_contents(),
            webui_bubble_manager_->contents_warmup_level()));
    bubble_created_time_.reset();
  }
}

void TabSearchBubbleHost::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(webui_bubble_manager_->GetBubbleWidget(), widget);
  DCHECK(bubble_widget_observation_.IsObservingSource(
      webui_bubble_manager_->GetBubbleWidget()));
  bubble_widget_observation_.Reset();
  pressed_lock_.reset();
}

void TabSearchBubbleHost::OnOrganizationAccepted(const Browser* browser) {
  if (browser != GetBrowser()) {
    return;
  }
  // Don't show IPH if the user already has other tab groups.
  if (browser->tab_strip_model()->group_model()->ListTabGroups().size() > 1) {
    return;
  }
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHTabOrganizationSuccessFeature);
}

void TabSearchBubbleHost::OnUserInvokedFeature(const Browser* browser) {
  if (browser == GetBrowser()) {
    const int tab_organization_tab_index = 1;
    ShowTabSearchBubble(
        false, tab_organization_tab_index,
        tab_search::mojom::TabOrganizationFeature::kAutoTabGroups);
  }
}

void TabSearchBubbleHost::BeforeBubbleWidgetShowed(views::Widget* widget) {
  CHECK_EQ(widget, webui_bubble_manager_->GetBubbleWidget());
  // There should only ever be a single bubble widget active for the
  // TabSearchBubbleHost.
  DCHECK(!bubble_widget_observation_.IsObserving());
  bubble_widget_observation_.Observe(widget);
  widget_open_timer_.Reset(widget);

  // The declutter controller is set in the WebUI controller, which notifies the
  // page handler. This works because the contents wrapper is created by the
  // `webui_bubble_manager_` during `webui_bubble_manager_->ShowBubble()`.
  // TODO (b/360724768): Refactor how WebUI page handlers can access specific
  // contexts more efficiently.
  CHECK(webui_bubble_manager_->GetContentsWrapper());
  CHECK(webui_bubble_manager_->GetContentsWrapper()->web_contents());
  content::WebUI* web_ui =
      webui_bubble_manager_->GetContentsWrapper()->web_contents()->GetWebUI();

  CHECK(web_ui);
  web_ui->GetController()->GetAs<TabSearchUI>()->InstallTabDeclutterController(
      GetBrowser()->GetFeatures().tab_declutter_controller());

  widget->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindOnce(
          [](base::TimeTicks button_pressed_time,
             const viz::FrameTimingDetails& frame_timing_details) {
            base::TimeTicks presentation_timestamp =
                frame_timing_details.presentation_feedback.timestamp;
            base::UmaHistogramMediumTimes(
                "Tabs.TabSearch."
                "ButtonPressedToNextFramePresented",
                presentation_timestamp - button_pressed_time);
          },
          base::TimeTicks::Now()));
}

bool TabSearchBubbleHost::ShowTabSearchBubble(
    bool triggered_by_keyboard_shortcut,
    int tab_index,
    tab_search::mojom::TabOrganizationFeature organization_feature) {
  TRACE_EVENT0("ui", "TabSearchBubbleHost::ShowTabSearchBubble");
  base::trace_event::EmitNamedTrigger("show-tab-search-bubble");
  if (tab_index >= 0) {
    profile_->GetPrefs()->SetInteger(tab_search_prefs::kTabSearchTabIndex,
                                     tab_index);
  }

  if (organization_feature !=
      tab_search::mojom::TabOrganizationFeature::kNone) {
    profile_->GetPrefs()->SetInteger(
        tab_search_prefs::kTabOrganizationFeature,
        tab_search_prefs::GetIntFromTabOrganizationFeature(
            organization_feature));
  }

  if (webui_bubble_manager_->GetBubbleWidget()) {
    return false;
  }

  // Close the Tab Search IPH if it is showing.
  if (auto* const browser = GetBrowser()) {
    browser->window()->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHTabSearchFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }

  std::optional<gfx::Rect> anchor;
  if (button_->GetWidget()->IsFullscreen() && !button_->IsDrawn()) {
    // Use a screen-coordinate anchor rect when the tabstrip's search button is
    // not drawn, and potentially positioned offscreen, in fullscreen mode.
    // Place the anchor similar to where the button would be in non-fullscreen
    // mode.
    const gfx::Rect bounds = button_->GetWidget()->GetWorkAreaBoundsInScreen();
    const int offset = GetLayoutConstant(TAB_STRIP_PADDING);

    const int x = tabs::GetTabSearchTrailingTabstrip(profile_)
                      ? bounds.right() - offset
                      : bounds.x() + offset;

    anchor.emplace(gfx::Rect(x, bounds.y() + offset, 0, 0));
  }

  bubble_created_time_ = base::TimeTicks::Now();
  webui_bubble_manager_->set_widget_initialization_callback(base::BindOnce(
      [](base::TimeTicks bubble_init_start_time) {
        base::UmaHistogramMediumTimes(
            "Tabs.TabSearch.BubbleWidgetInitializationTime",
            base::TimeTicks::Now() - bubble_init_start_time);
      },
      *bubble_created_time_));
  webui_bubble_manager_->ShowBubble(anchor,
                                    tabs::GetTabSearchTrailingTabstrip(profile_)
                                        ? views::BubbleBorder::TOP_RIGHT
                                        : views::BubbleBorder::TOP_LEFT,
                                    kTabSearchBubbleElementId);

  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile_);
  if (tracker)
    tracker->NotifyEvent(feature_engagement::events::kTabSearchOpened);

  if (triggered_by_keyboard_shortcut) {
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  TabSearchOpenAction::kKeyboardShortcut);
  }

  // Hold the pressed lock while the |bubble_| is active.
  pressed_lock_ = menu_button_controller_->TakeLock();
  return true;
}

void TabSearchBubbleHost::CloseTabSearchBubble() {
  webui_bubble_manager_->CloseBubble();
}

Browser* TabSearchBubbleHost::GetBrowser() {
  for (Browser* browser : chrome::FindAllBrowsersWithProfile(profile_)) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (browser_view->GetTabSearchBubbleHost() == this) {
      return browser;
    }
  }
  return nullptr;
}

void TabSearchBubbleHost::ButtonPressed(const ui::Event& event) {
  if (ShowTabSearchBubble()) {
    // Only log the open action if it resulted in creating a new instance of the
    // Tab Search bubble.
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  GetActionForEvent(event));
    return;
  }
  CloseTabSearchBubble();
}
