// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"

#include "base/functional/bind.h"
#include "base/system/sys_info.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_url_matcher.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/grit/branded_strings.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"

namespace {

// TODO(crbug.com/382494946): Similar bespoke checks are used throughout the
// codebase. This should be factored out as a common util and other callsites
// converted to use this.
bool IsNewTabPage(content::WebContents* const web_contents) {
  // Use the committed entry (or the visible entry, if the committed entry is
  // the initial NavigationEntry).
  CHECK(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    entry = web_contents->GetController().GetVisibleEntry();
  }
  const GURL& url = entry->GetURL();
  return NewTabUI::IsNewTab(url) || NewTabPageUI::IsNewTabPageOrigin(url) ||
         NewTabPageThirdPartyUI::IsNewTabPageOrigin(url) ||
         search::NavEntryIsInstantNTP(web_contents, entry);
}

}  // namespace

namespace lens {

LensOverlayEntryPointController::LensOverlayEntryPointController() = default;

void LensOverlayEntryPointController::Initialize(
    BrowserWindowInterface* browser_window_interface,
    CommandUpdater* command_updater,
    views::View* location_bar) {
  browser_window_interface_ = browser_window_interface;
  location_bar_ = location_bar;
  if (location_bar_) {
    focus_manager_observation_.Observe(location_bar_->GetFocusManager());
    location_bar_->AddObserver(this);
  }

  pref_change_registrar_.Init(
      browser_window_interface_->GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      omnibox::kShowGoogleLensShortcut,
      base::BindRepeating(
          &LensOverlayEntryPointController::UpdatePageActionState,
          base::Unretained(this)));
  command_updater_ = command_updater;

  // Observe changes to fullscreen state.
  fullscreen_observation_.Observe(
      browser_window_interface_->GetExclusiveAccessManager()
          ->fullscreen_controller());

  // Observe changes to user's DSE.
  if (auto* const template_url_service =
          TemplateURLServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile())) {
    template_url_service_observation_.Observe(template_url_service);
  }

  // Update all entry points.
  UpdateEntryPointsState(/*hide_if_needed=*/true);

  edu_url_matcher_ = std::make_unique<lens::LensUrlMatcher>(
      lens::features::GetLensOverlayEduUrlAllowFilters(),
      lens::features::GetLensOverlayEduUrlBlockFilters(),
      lens::features::GetLensOverlayEduUrlPathMatchAllowFilters(),
      lens::features::GetLensOverlayEduUrlPathMatchBlockFilters(),
      lens::features::GetLensOverlayEduUrlForceAllowedMatchPatterns(),
      lens::features::GetLensOverlayEduHashedDomainBlockFilters());
}

LensOverlayEntryPointController::~LensOverlayEntryPointController() {
  // Initialize may not have been called (e.g. for non-normal browser windows).
  if (location_bar_) {
    location_bar_->RemoveObserver(this);
  }
}

bool LensOverlayEntryPointController::IsEnabled() const {
  // This class is initialized if and only if it is observing.
  if (!fullscreen_observation_.IsObserving()) {
    return false;
  }

  // Feature is disabled via finch.
  if (!lens::features::IsLensOverlayEnabled()) {
    return false;
  }

  // Disable in fullscreen without top-chrome.
  if (!lens::features::GetLensOverlayEnableInFullscreen() &&
      browser_window_interface_->GetExclusiveAccessManager()
          ->context()
          ->IsFullscreen() &&
      !browser_window_interface_->IsTabStripVisible()) {
    return false;
  }

  const PrefService* pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  // Lens Overlay is disabled via the legacy enterprise policy.
  lens::prefs::LensOverlaySettingsPolicyValue old_policy_value =
      static_cast<lens::prefs::LensOverlaySettingsPolicyValue>(
          pref_service->GetInteger(lens::prefs::kLensOverlaySettings));
  if (old_policy_value ==
      lens::prefs::LensOverlaySettingsPolicyValue::kDisabled) {
    return false;
  }

  // Lens Overlay is disabled via the GenAI enterprise policy.
  lens::prefs::GenAiLensOverlaySettingsPolicyValue policy_value =
      static_cast<lens::prefs::GenAiLensOverlaySettingsPolicyValue>(
          pref_service->GetInteger(lens::prefs::kGenAiLensOverlaySettings));
  if (policy_value ==
      lens::prefs::GenAiLensOverlaySettingsPolicyValue::kDisabled) {
    // Disabled via the enterprise policy.
    return false;
  }

  // Lens Overlay is only enabled if the user's default search engine is Google.
  if (lens::features::IsLensOverlayGoogleDseRequired() &&
      !search::DefaultSearchProviderIsGoogle(
          browser_window_interface_->GetProfile())) {
    return false;
  }

  // Finally, only enable the overlay if user meets our minimum RAM requirement.
  static int phys_mem_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  return phys_mem_mb > lens::features::GetLensOverlayMinRamMb();
}

bool LensOverlayEntryPointController::AreVisible() const {
  return IsEnabled() && !IsOverlayActive();
}

void LensOverlayEntryPointController::UpdateEntryPointsState(
    bool hide_toolbar_entrypoint) {
  const bool enabled = IsEnabled();
  const bool visible = AreVisible();

  // Update the 3 dot menu entry point.
  command_updater_->UpdateCommandEnabled(IDC_CONTENT_CONTEXT_LENS_OVERLAY,
                                         visible);

  // Update the pinnable toolbar entry point. Toolbar entry point is always
  // present, therefore, ignore the visibility check.
  if (auto* const toolbar_entry_point = GetToolbarEntrypoint()) {
    toolbar_entry_point->SetEnabled(enabled);
    if (hide_toolbar_entrypoint) {
      toolbar_entry_point->SetVisible(enabled);
    }
  }
  UpdatePageActionState();
}

bool LensOverlayEntryPointController::IsUrlEduEligible(const GURL& url) const {
  if (!IsEnabled()) {
    return false;
  }
  return edu_url_matcher_->IsMatch(url);
}

// static
void LensOverlayEntryPointController::InvokeAction(
    tabs::TabInterface* active_tab,
    const actions::ActionInvocationContext& context) {
  LensSearchController* search_controller =
      active_tab->GetTabFeatures()->lens_search_controller();
  LensOverlayController* overlay_controller =
      active_tab->GetTabFeatures()->lens_overlay_controller();

  std::underlying_type_t<page_actions::PageActionTrigger> page_action_trigger =
      context.GetProperty(page_actions::kPageActionTriggerKey);
  // The Lens Overlay action item has different entry points that may trigger it
  // (e.g., toolbar, page action, etc.). Triggers from a page action will have a
  // valid PageActionTrigger property set.
  if (page_action_trigger != page_actions::kInvalidPageActionTrigger) {
    switch (static_cast<page_actions::PageActionTrigger>(page_action_trigger)) {
      case page_actions::PageActionTrigger::kKeyboard:
        active_tab->GetBrowserWindowInterface()
            ->GetFeatures()
            .lens_region_search_controller()
            ->Start(active_tab->GetContents(), /*use_fullscreen_capture=*/true,
                    /*is_google_default_search_provider=*/true,
                    lens::AmbientSearchEntryPoint::
                        LENS_OVERLAY_LOCATION_BAR_ACCESSIBILITY_FALLBACK);

        break;
      default:
        lens::RecordAmbientSearchQuery(
            lens::AmbientSearchEntryPoint::LENS_OVERLAY_LOCATION_BAR);
        search_controller->OpenLensOverlay(
            lens::LensOverlayInvocationSource::kOmnibox);
        active_tab->GetBrowserWindowInterface()
            ->GetUserEducationInterface()
            ->NotifyNewBadgeFeatureUsed(lens::features::kLensOverlay);
    }
    return;
  }

  // Toggle the Lens overlay. There's no need to show or hide the side
  // panel as the overlay controller will handle that.
  if (overlay_controller->IsOverlayActive()) {
    search_controller->CloseLensAsync(
        lens::LensOverlayDismissalSource::kToolbar);
  } else {
    search_controller->OpenLensOverlay(
        lens::LensOverlayInvocationSource::kToolbar);
  }
}

void LensOverlayEntryPointController::OnViewAddedToWidget(views::View* view) {
  CHECK(location_bar_);
  focus_manager_observation_.Observe(location_bar_->GetFocusManager());
}

void LensOverlayEntryPointController::OnViewRemovedFromWidget(
    views::View* view) {
  CHECK(location_bar_);
  CHECK(location_bar_->GetFocusManager());
  focus_manager_observation_.Reset();
}

void LensOverlayEntryPointController::OnDidChangeFocus(views::View* before,
                                                       views::View* now) {
  UpdatePageActionState();
}

void LensOverlayEntryPointController::OnFullscreenStateChanged() {
  // Disable the Lens entry points in the top chrome if there is no top bar in
  // Chrome. On Mac and ChromeOS, it is possible to hover over the top of the
  // screen to get the top bar back, but since does top bar does not stay
  // open, we need to disable those entry points.
  UpdateEntryPointsState(/*hide_toolbar_entrypoint=*/false);
}

void LensOverlayEntryPointController::OnTemplateURLServiceChanged() {
  // Possibly add/remove the entrypoints based on the new users default search
  // engine.
  UpdateEntryPointsState(/*hide_toolbar_entrypoint=*/true);
}

void LensOverlayEntryPointController::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
}

actions::ActionItem* LensOverlayEntryPointController::GetToolbarEntrypoint() {
  return actions::ActionManager::Get().FindAction(
      kActionSidePanelShowLensOverlayResults,
      /*scope=*/browser_window_interface_->GetActions()->root_action_item());
}

void LensOverlayEntryPointController::UpdatePageActionState() {
  if (!IsPageActionMigrated(PageActionIconType::kLensOverlay)) {
    return;
  }
  // This may not have been initialized (e.g. for non-normal browser types).
  if (!location_bar_) {
    return;
  }
  CHECK(browser_window_interface_);

  tabs::TabInterface* active_tab =
      browser_window_interface_->GetActiveTabInterface();
  // Possible during browser window initialization or teardown.
  if (!active_tab) {
    return;
  }
  CHECK(active_tab->GetTabFeatures());

  page_actions::PageActionController* page_action_controller =
      active_tab->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);

  const actions::ActionId page_action_id =
      kActionSidePanelShowLensOverlayResults;

  if (!ShouldShowPageAction(active_tab)) {
    page_action_controller->Hide(page_action_id);
    return;
  }

  // No-ops if the overriding string is the same.
  page_action_controller->OverrideText(
      page_action_id,
      l10n_util::GetStringUTF16(IDS_CONTENT_LENS_OVERLAY_ENTRYPOINT_LABEL));

  page_action_controller->Show(page_action_id);
  page_action_controller->ShowSuggestionChip(page_action_id,
                                             {
                                                 .should_animate = false,
                                             });
}

bool LensOverlayEntryPointController::IsOverlayActive() const {
  const auto* active_tab = browser_window_interface_->GetActiveTabInterface();
  if (!active_tab) {
    return false;
  }
  const auto* controller =
      active_tab->GetTabFeatures()->lens_overlay_controller();
  return controller && controller->IsOverlayActive();
}

bool LensOverlayEntryPointController::ShouldShowPageAction(
    tabs::TabInterface* active_tab) const {
  if (!AreVisible()) {
    return false;
  }

  if (!browser_window_interface_->GetProfile()->GetPrefs()->GetBoolean(
          omnibox::kShowGoogleLensShortcut)) {
    return false;
  }

  if (!features::IsOmniboxEntryPointEnabled()) {
    return false;
  }

  if (!features::IsOmniboxEntrypointAlwaysVisible() &&
      !location_bar_->Contains(
          location_bar_->GetFocusManager()->GetFocusedView())) {
    return false;
  }

  // The overlay is unavailable on the NTP as it is unlikely to be useful to
  // users on the page. It would also appear immediately when a new tab or
  // window is created due to focus immediately jumping into the location bar.
  if (IsNewTabPage(active_tab->GetContents())) {
    return false;
  }

  return true;
}

}  // namespace lens
