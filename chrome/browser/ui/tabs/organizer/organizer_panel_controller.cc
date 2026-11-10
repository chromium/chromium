// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"

#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/organizer/organizer_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

namespace {

// Respond to vertical tab strip collapse by hiding the panel.
void OnVerticalTabStripAnimation(
    OrganizerPanelController* panel_controller,
    const BrowserAnimationController* animation_controller,
    BrowserAnimationUpdate update) {
  if (update != BrowserAnimationUpdate::kStarted) {
    return;
  }
  const auto motion = animation_controller->GetCurrentMotion(
      TabStripAnimations::kVerticalTabStrip);
  if (motion == TabStripAnimations::kCollapseOnHover ||
      motion == TabStripAnimations::kCollapse) {
    panel_controller->SetOrganizerVisible(false);
  }
}

// LINT.IfChange(OrganizerSection)
std::string_view GetHistogramSectionName(organizer::SectionId section_id) {
  switch (section_id) {
    case organizer::SectionId::kCrossDeviceTabs:
      return "CrossDeviceTabs";
    case organizer::SectionId::kOpenTabs:
      return "OpenTabs";
    case organizer::SectionId::kRecentlyClosed:
      return "RecentlyClosed";
    case organizer::SectionId::kTabGroups:
      return "TabGroups";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/histograms.xml:OrganizerSection)

bool ShouldRecordSectionMetrics(Profile& profile,
                                organizer::SectionId section_id) {
  switch (section_id) {
    case organizer::SectionId::kCrossDeviceTabs: {
      if (!organizer_panel::IsOrganizerPanelForeignTabsEnabled()) {
        return false;
      }
      signin::IdentityManager* const identity_manager =
          IdentityManagerFactory::GetForProfile(&profile);
      return identity_manager &&
             identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
    }
    case organizer::SectionId::kRecentlyClosed:
      return !profile.IsIncognitoProfile();
    case organizer::SectionId::kOpenTabs:
    case organizer::SectionId::kTabGroups:
      return true;
  }
}

void RecordSectionStateMetricsOnOpen(Profile& profile) {
  PrefService* const prefs = profile.GetPrefs();
  CHECK(prefs);
  for (const auto& entry : organizer::GetAllSectionStates(*prefs)) {
    if (!ShouldRecordSectionMetrics(profile, entry.section_id)) {
      continue;
    }
    const std::string_view section_name =
        GetHistogramSectionName(entry.section_id);
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Tabs.Organizer.Section.", section_name, ".ExpandedStateOnOpen"}),
        entry.state.expanded);
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Tabs.Organizer.Section.", section_name, ".ShowAllStateOnOpen"}),
        entry.state.show_all);
  }
}

}  // namespace

DEFINE_USER_DATA(OrganizerPanelController);

// Handles the work of moving the panel view around to the correct host based on
// browser state. This will normally be in the organizer tray view or the
// vertical tab strip region view.
class OrganizerPanelController::PanelViewManager {
 public:
  PanelViewManager(OrganizerPanelController& controller,
                   BrowserWindowInterface& browser)
      : controller_(controller), browser_(browser) {
    if (auto* const state_controller =
            tabs::VerticalTabStripStateController::From(&*browser_)) {
      auto callback =
          base::BindRepeating(&PanelViewManager::OnVerticalTabStripModeChanged,
                              base::Unretained(this));
      tab_strip_subscriptions_.emplace_back(
          state_controller->RegisterOnModeChanged(
              base::IgnoreArgs<tabs::VerticalTabStripStateController*>(
                  callback)));
      tab_strip_subscriptions_.emplace_back(
          state_controller->RegisterOnCollapseChanged(
              base::IgnoreArgs<tabs::VerticalTabStripCollapseState>(callback)));
      tab_strip_subscriptions_.emplace_back(
          state_controller->RegisterOnExpandOnHoverEnabledChanged(
              base::IgnoreArgs<bool>(callback)));
    }
  }

  void OnVerticalTabStripModeChanged() {
    if (UpdatePanelViewHost()) {
      if (controller_->IsOrganizerPanelVisible()) {
        controller_->SetOrganizerVisible(false, /*immediate=*/true);
      } else {
        controller_->NotifyStateChanged();
      }
    }
  }

  std::unique_ptr<views::View> SetPanelView(
      std::unique_ptr<views::View> panel_view) {
    const auto old_location = current_location_;
    std::unique_ptr<views::View> old_panel = RemovePanelView();
    if (panel_view) {
      AddPanelView(std::move(panel_view));
    }
    if (current_location_ != old_location) {
      controller_->NotifyStateChanged();
    }
    return old_panel;
  }

  // Maybe moves the panel between hosts if the desired host has changed.
  // Returns true if changed, false otherwise.
  bool UpdatePanelViewHost() {
    if (!panel_view_) {
      return false;
    }

    // Determine where the panel should go.
    const auto new_location =
        OrganizerPanelHost::GetPreferredLocation(*browser_);
    CHECK_NE(OrganizerPanelLocation::kNone, new_location)
        << "No new host to move panel to.";
    if (current_location_ == new_location) {
      return false;
    }

    // Determine where the panel currently is.
    auto* const current_host = OrganizerPanelHost::FromView(panel_view_.view());
    CHECK(current_host) << "No current host to move panel from.";
    DCHECK_EQ(
        OrganizerPanelHost::GetHostForLocation(*browser_, current_location_),
        current_host)
        << "Sanity check failed: Current host is not expected host.";

    // Find the new host.
    auto* const desired_host =
        OrganizerPanelHost::GetHostForLocation(*browser_, new_location);
    CHECK(desired_host) << "Browser has no panel host.";
    CHECK_NE(desired_host, current_host)
        << "Location changed but host remained the same.";

    // Move the panel.
    current_location_ = new_location;
    desired_host->SetOrganizerPanelView(current_host->TakeOrganizerPanelView());
    return true;
  }

  OrganizerPanelLocation current_location() const { return current_location_; }

 private:
  // Removes the panel view from its current host.
  std::unique_ptr<views::View> RemovePanelView() {
    auto* const panel_view = panel_view_.view();
    panel_view_.SetView(nullptr);
    current_location_ = OrganizerPanelLocation::kNone;
    if (!panel_view) {
      return nullptr;
    }
    auto* const old_host = OrganizerPanelHost::FromView(panel_view);
    CHECK(old_host) << "Panel was present but not in a host.";
    return old_host->TakeOrganizerPanelView();
  }

  // Adds the panel view to the correct host for the current browser state.
  void AddPanelView(std::unique_ptr<views::View> panel_view) {
    CHECK(panel_view);
    CHECK(!panel_view_);
    CHECK_EQ(OrganizerPanelLocation::kNone, current_location_);
    current_location_ = OrganizerPanelHost::GetPreferredLocation(*browser_);
    CHECK_NE(OrganizerPanelLocation::kNone, current_location_)
        << "Browser has no panel host";
    panel_view_.SetView(panel_view.get());
    auto* const host =
        OrganizerPanelHost::GetHostForLocation(*browser_, current_location_);
    CHECK(host) << "Panel host is missing.";
    host->SetOrganizerPanelView(std::move(panel_view));
  }

  const raw_ref<OrganizerPanelController> controller_;
  const raw_ref<BrowserWindowInterface> browser_;
  std::vector<base::CallbackListSubscription> tab_strip_subscriptions_;
  views::ViewTracker panel_view_;
  OrganizerPanelLocation current_location_ = OrganizerPanelLocation::kNone;
};

OrganizerPanelController::OrganizerPanelController(
    BrowserWindowInterface& browser_window,
    actions::ActionItem* root_action_item)
    : browser_window_(browser_window),
      root_action_item_(root_action_item),
      panel_view_manager_(
          std::make_unique<PanelViewManager>(*this, browser_window)),
      scoped_unowned_user_data_(browser_window.GetUnownedUserDataHost(),
                                *this) {
  UpdateOrganizerActionItem();

  vertical_tab_strip_animation_subscription_ =
      BrowserAnimationController::From(&browser_window)
          ->Subscribe(TabStripAnimations::kVerticalTabStrip,
                      base::BindRepeating(&OnVerticalTabStripAnimation,
                                          base::Unretained(this)));
}

OrganizerPanelController::~OrganizerPanelController() {
  if (is_visible_) {
    RecordCloseMetrics();
  }
}

// static
OrganizerPanelController* OrganizerPanelController::From(
    BrowserWindowInterface* browser_window) {
  return Get(browser_window->GetUnownedUserDataHost());
}

bool OrganizerPanelController::IsOrganizerPanelVisible() const {
  return is_visible_;
}

bool OrganizerPanelController::IsOrganizerPanelEmbeddedInVerticalTabStrip()
    const {
  return IsOrganizerPanelVisible() &&
         (GetCurrentOrganizerPanelLocation() ==
              OrganizerPanelLocation::kVerticalTabStrip ||
          (GetCurrentOrganizerPanelLocation() ==
               OrganizerPanelLocation::kNone &&
           OrganizerPanelHost::GetPreferredLocation(*browser_window_) ==
               OrganizerPanelLocation::kVerticalTabStrip));
}

void OrganizerPanelController::SetOrganizerVisible(bool visible,
                                                   bool immediate) {
  if (is_visible_ == visible) {
    return;
  }

  if (auto* const vertical_tabs =
          tabs::VerticalTabStripStateController::From(&*browser_window_);
      visible && vertical_tabs && vertical_tabs->IsResizing()) {
    // Cannot show during vertical tab strip resize.
    return;
  }

  is_visible_ = visible;
  base::RecordAction(base::UserMetricsAction(
      is_visible_ ? "OrganizerPanel.Open" : "OrganizerPanel.Close"));
  if (is_visible_) {
    close_action_ = OrganizerPanelCloseAction::kNoAction;
    num_tabs_closed_ = 0;
    if (auto* const interface =
            BrowserUserEducationInterface::From(&*browser_window_)) {
      const base::Feature& iph_feature =
          feature_engagement::kIPH_VerticalTabsTabGroupsInOrganizerFeature;
      if (interface->IsFeaturePromoActive(iph_feature)) {
        interface->NotifyFeaturePromoFeatureUsed(
            iph_feature, FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
      } else if (interface->IsFeaturePromoQueued(iph_feature)) {
        interface->AbortFeaturePromo(iph_feature);
      }
    }
    UpdatePanelWebContents();
    Profile* const profile = browser_window_->GetProfile();
    CHECK(profile);
    RecordSectionStateMetricsOnOpen(*profile);
  } else {
    RecordCloseMetrics();
  }
  if (immediate) {
    BrowserAnimationController::From(&*browser_window_)
        ->Reset(OrganizerPanelAnimations::kOrganizerPanel, GetTargetMotion());
  } else {
    BrowserAnimationController::From(&*browser_window_)
        ->Start(OrganizerPanelAnimations::kOrganizerPanel, GetTargetMotion());
  }

  NotifyStateChanged();
}

void OrganizerPanelController::SetCloseAction(
    OrganizerPanelCloseAction action) {
  if (!is_visible_) {
    return;
  }
  close_action_ = action;
}

void OrganizerPanelController::IncrementNumTabsClosed(uint32_t count) {
  if (!is_visible_) {
    return;
  }
  num_tabs_closed_ = base::ClampAdd<int>(num_tabs_closed_, count);
}

void OrganizerPanelController::RecordCloseMetrics() {
  base::UmaHistogramEnumeration("Tabs.Organizer.CloseAction", close_action_);
  base::UmaHistogramCounts100("Tabs.Organizer.NumTabsClosedPerInstance",
                              num_tabs_closed_);
  close_action_ = OrganizerPanelCloseAction::kNoAction;
  num_tabs_closed_ = 0;
}

OrganizerPanelLocation
OrganizerPanelController::GetCurrentOrganizerPanelLocation() const {
  return panel_view_manager_->current_location();
}

BrowserAnimationMotion OrganizerPanelController::GetTargetMotion() const {
  bool vts_enabled = false;
  if (auto* const vts_controller =
          tabs::VerticalTabStripStateController::From(&*browser_window_)) {
    vts_enabled = vts_controller->ShouldDisplayVerticalTabs();
  }
  if (vts_enabled) {
    if (panel_view_manager_->current_location() ==
        OrganizerPanelLocation::kVerticalTabStrip) {
      return is_visible_ ? OrganizerPanelAnimations::kShowInVerticalTabs
                         : OrganizerPanelAnimations::kHideInVerticalTabs;
    }
    return is_visible_ ? OrganizerPanelAnimations::kShowExpandFromVerticalTabs
                       : OrganizerPanelAnimations::kHideExpandFromVerticalTabs;
  }
  return is_visible_ ? OrganizerPanelAnimations::kShowInTray
                     : OrganizerPanelAnimations::kHideInTray;
}

base::CallbackListSubscription OrganizerPanelController::RegisterOnStateChanged(
    StateChangedCallback callback) {
  return on_state_changed_callback_list_.Add(std::move(callback));
}

content::WebContents* OrganizerPanelController::EnsureWebContentsCreated() {
  if (web_contents_) {
    return web_contents_.get();
  }

  auto* const preload_manager = WebUIContentsPreloadManager::GetInstance();
  CHECK(preload_manager);
  WebUIContentsPreloadManager::RequestResult result = preload_manager->Request(
      GURL(chrome::kChromeUIOrganizerPanelURL), browser_window_->GetProfile());
  web_contents_ = std::move(result.web_contents);
  webui::SetBrowserWindowInterface(web_contents_.get(), &*browser_window_);
  web_contents_->SetSupportsDraggableRegions(true);
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement_helper_ =
        std::make_unique<site_engagement::SiteEngagementService::Helper>(
            web_contents_.get());
  }
  return web_contents_.get();
}

void OrganizerPanelController::RegisterPanelView(
    OrganizerPanelView* panel_view) {
  CHECK(!panel_view_);
  CHECK(!is_visible_);
  panel_view_ = panel_view;
}

void OrganizerPanelController::UnregisterPanelView(
    OrganizerPanelView* panel_view) {
  CHECK_EQ(panel_view_, panel_view);
  panel_view_ = nullptr;
}

void OrganizerPanelController::UpdatePanelWebContents() {
  if (!is_visible_ || !panel_view_) {
    return;
  }
  if (content::WebContents* const contents = EnsureWebContentsCreated()) {
    panel_view_->AttachWebContents(contents);
  }
}

std::unique_ptr<views::View> OrganizerPanelController::SetPanelView(
    base::PassKey<BrowserView>,
    std::unique_ptr<views::View> panel_view) {
  return panel_view_manager_->SetPanelView(std::move(panel_view));
}

std::unique_ptr<views::View> OrganizerPanelController::SetPanelViewForTesting(
    std::unique_ptr<views::View> panel_view) {
  return panel_view_manager_->SetPanelView(std::move(panel_view));
}

void OrganizerPanelController::NotifyStateChanged() {
  UpdateOrganizerActionItem();
  on_state_changed_callback_list_.Notify(this);
}

void OrganizerPanelController::UpdateOrganizerActionItem() {
  actions::ActionItem* organizer_action =
      actions::ActionManager::Get().FindAction(kActionToggleOrganizerPanel,
                                               root_action_item_);
  if (!organizer_action) {
    return;
  }

  const bool is_panel_in_vertical_strip =
      IsOrganizerPanelEmbeddedInVerticalTabStrip();
  const auto& text = is_panel_in_vertical_strip
                         ? IDS_ORGANIZER_PANEL_CLOSE_TAB_SEARCH
                         : IDS_TAB_SEARCH_MENU;
  const std::u16string title_and_tooltip =
      chrome::GetCleanText(l10n_util::GetStringUTF16(text));
  organizer_action->SetText(title_and_tooltip);
  organizer_action->SetTooltipText(title_and_tooltip);

  const gfx::VectorIcon& icon =
      is_panel_in_vertical_strip
          ? (features::IsRoundedIconsEnabled() ? kCloseSmallIcon
                                               : kCloseChromeRefreshOldIcon)
          : (features::IsRoundedIconsEnabled() ? kManageSearchIcon
                                               : kTabSearchTabStripOldIcon);
  organizer_action->SetImage(
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon));
}
