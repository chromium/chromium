// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"

#include <memory>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace {

// Should be kept in sync with histogram enum
// CompanionSidePanelAvailabilityChanged.
enum class CompanionSidePanelAvailabilityChanged {
  kUnavailableToUnavailable = 0,
  kUnavailableToAvailable = 1,
  kAvailableToUnavailable = 2,
  kAvailableToAvailable = 3,
  kMaxValue = kAvailableToAvailable
};

}  // namespace

SearchCompanionSidePanelCoordinator::SearchCompanionSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<SearchCompanionSidePanelCoordinator>(*browser),
      browser_(browser),
      accessible_name_(
          l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_PANEL_COMPANION_SHOW)),
      // TODO(b/269331995): Localize menu item label.
      name_(l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMPANION_TITLE)),
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      icon_(
          vector_icons::kGoogleSearchCompanionMonochromeLogoChromeRefreshIcon),
      disabled_icon_(
          vector_icons::kGoogleSearchCompanionMonochromeLogoChromeRefreshIcon),
#else
      icon_(vector_icons::kSearchIcon),
      disabled_icon_(vector_icons::kSearchIcon),
#endif
      pref_service_(browser->profile()->GetPrefs()) {
  if (auto* template_url_service =
          TemplateURLServiceFactory::GetForProfile(browser->profile())) {
    template_url_service_observation_.Observe(template_url_service);
  }
  // Only start observing tab changes if google is the default search provider.
  if (companion::IsSearchInCompanionSidePanelSupported(browser)) {
    is_currently_observing_tab_changes_ = true;
    browser_->tab_strip_model()->AddObserver(this);
    CreateAndRegisterEntriesForExistingWebContents(browser_->tab_strip_model());
  }

  policy_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  policy_pref_change_registrar_->Init(pref_service_);
  policy_pref_change_registrar_->Add(
      prefs::kGoogleSearchSidePanelEnabled,
      base::BindRepeating(
          &SearchCompanionSidePanelCoordinator::OnPolicyPrefChanged,
          base::Unretained(this)));

  if (base::FeatureList::IsEnabled(
          companion::features::internal::
              kCompanionEnabledByObservingExpsNavigations)) {
    exps_optin_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    exps_optin_pref_change_registrar_->Init(pref_service_);
    exps_optin_pref_change_registrar_->Add(
        companion::kHasNavigatedToExpsSuccessPage,
        base::BindRepeating(
            &SearchCompanionSidePanelCoordinator::OnExpsPolicyPrefChanged,
            base::Unretained(this)));
  }
}

SearchCompanionSidePanelCoordinator::~SearchCompanionSidePanelCoordinator() =
    default;

// static
bool SearchCompanionSidePanelCoordinator::IsSupported(
    Profile* profile,
    bool include_runtime_checks) {
  return companion::IsSearchInCompanionSidePanelSupportedForProfile(
      profile, include_runtime_checks);
}

bool SearchCompanionSidePanelCoordinator::Show(
    SidePanelOpenTrigger side_panel_open_trigger) {
  GetBrowser().GetFeatures().side_panel_ui()->Show(
      SidePanelEntry::Id::kSearchCompanion, side_panel_open_trigger);
  return true;
}

void SearchCompanionSidePanelCoordinator::ShowLens(
    const content::OpenURLParams& url_params) {
  // First, we need to modify our view to make sure we are showing Lens.
  auto* companion_tab_helper = companion::CompanionTabHelper::FromWebContents(
      browser_->tab_strip_model()->GetActiveWebContents());
  companion_tab_helper->OpenContextualLensView(url_params);
  GetBrowser().GetFeatures().side_panel_ui()->Show(
      SidePanelEntry::Id::kSearchCompanion,
      SidePanelOpenTrigger::kLensContextMenu);
}

BrowserView* SearchCompanionSidePanelCoordinator::GetBrowserView() const {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

std::u16string SearchCompanionSidePanelCoordinator::GetTooltipForToolbarButton()
    const {
  return l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMPANION_TOOLBAR_TOOLTIP);
}

void SearchCompanionSidePanelCoordinator::NotifyCompanionOfSidePanelOpenTrigger(
    std::optional<SidePanelOpenTrigger> side_panel_open_trigger) {
  auto* companion_tab_helper = companion::CompanionTabHelper::FromWebContents(
      browser_->tab_strip_model()->GetActiveWebContents());
  companion_tab_helper->SetMostRecentSidePanelOpenTrigger(
      side_panel_open_trigger);
}

void SearchCompanionSidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::Type::kInserted) {
    for (const auto& inserted_tab : change.GetInsert()->contents) {
      companion::CompanionTabHelper::FromWebContents(inserted_tab.contents)
          ->CreateAndRegisterEntry();
    }
  }
  if (change.type() == TabStripModelChange::Type::kReplaced) {
    raw_ptr<content::WebContents> new_contents =
        change.GetReplace()->new_contents;
    if (new_contents) {
      companion::CompanionTabHelper::FromWebContents(new_contents)
          ->CreateAndRegisterEntry();
    }
  }
  if (selection.active_tab_changed()) {
    MaybeUpdateCompanionEnabledState();
  }
}

void SearchCompanionSidePanelCoordinator::TabChangedAt(
    content::WebContents* contents,
    int index,
    TabChangeType change_type) {
  MaybeUpdateCompanionEnabledState();
}

void SearchCompanionSidePanelCoordinator::
    CreateAndRegisterEntriesForExistingWebContents(
        TabStripModel* tab_strip_model) {
  for (int index = 0; index < tab_strip_model->GetTabCount(); index++) {
    companion::CompanionTabHelper::FromWebContents(
        tab_strip_model->GetWebContentsAt(index))
        ->CreateAndRegisterEntry();
  }
}

void SearchCompanionSidePanelCoordinator::
    DeregisterEntriesForExistingWebContents(TabStripModel* tab_strip_model) {
  for (int index = 0; index < tab_strip_model->GetTabCount(); index++) {
    companion::CompanionTabHelper::FromWebContents(
        tab_strip_model->GetWebContentsAt(index))
        ->DeregisterEntry();
  }
}

void SearchCompanionSidePanelCoordinator::OnTemplateURLServiceChanged() {
  UpdateCompanionAvailabilityInSidePanel();
}

void SearchCompanionSidePanelCoordinator::
    UpdateCompanionAvailabilityInSidePanel() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return;
  }

  // Update existence of companion entry points based on changes.
  if (companion::IsSearchInCompanionSidePanelSupported(browser_) &&
      !is_currently_observing_tab_changes_) {
    base::UmaHistogramEnumeration(
        "Companion.SidePanelAvailabilityChanged",
        CompanionSidePanelAvailabilityChanged::kUnavailableToAvailable);
    is_currently_observing_tab_changes_ = true;

    GetActionItem()->SetVisible(true);
    browser_->tab_strip_model()->AddObserver(this);
    CreateAndRegisterEntriesForExistingWebContents(browser_->tab_strip_model());
    return;
  }

  if (!companion::IsSearchInCompanionSidePanelSupported(browser_) &&
      is_currently_observing_tab_changes_) {
    base::UmaHistogramEnumeration(
        "Companion.SidePanelAvailabilityChanged",
        CompanionSidePanelAvailabilityChanged::kAvailableToUnavailable);
    is_currently_observing_tab_changes_ = false;

    GetActionItem()->SetVisible(false);
    browser_->tab_strip_model()->RemoveObserver(this);
    DeregisterEntriesForExistingWebContents(browser_->tab_strip_model());
    return;
  }

  if (companion::IsSearchInCompanionSidePanelSupported(browser_) &&
      is_currently_observing_tab_changes_) {
    base::UmaHistogramEnumeration(
        "Companion.SidePanelAvailabilityChanged",
        CompanionSidePanelAvailabilityChanged::kAvailableToAvailable);
    return;
  }

  if (!companion::IsSearchInCompanionSidePanelSupported(browser_) &&
      !is_currently_observing_tab_changes_) {
    base::UmaHistogramEnumeration(
        "Companion.SidePanelAvailabilityChanged",
        CompanionSidePanelAvailabilityChanged::kUnavailableToUnavailable);
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

actions::ActionItem* SearchCompanionSidePanelCoordinator::GetActionItem() {
  BrowserActions* browser_actions = browser_->browser_actions();
  return actions::ActionManager::Get().FindAction(
      kActionSidePanelShowSearchCompanion, browser_actions->root_action_item());
}

void SearchCompanionSidePanelCoordinator::MaybeUpdateCompanionEnabledState() {
  bool enabled = companion::IsCompanionAvailableForCurrentActiveTab(browser_);

  actions::ActionItem* action_item = GetActionItem();
  action_item->SetEnabled(enabled);
  action_item->SetImage(ui::ImageModel::FromVectorIcon(
      (enabled ? icon() : disabled_icon()), ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
}

void SearchCompanionSidePanelCoordinator::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
}

void SearchCompanionSidePanelCoordinator::OnPolicyPrefChanged() {
  if (!pref_service_) {
    return;
  }

  UpdateCompanionAvailabilityInSidePanel();
}

void SearchCompanionSidePanelCoordinator::OnExpsPolicyPrefChanged() {
  if (!pref_service_) {
    return;
  }
  base::UmaHistogramBoolean(
      "Companion.HasNavigatedToExpsSuccessPagePref.OnChanged",
      pref_service_->GetBoolean(companion::kHasNavigatedToExpsSuccessPage));

  UpdateCompanionAvailabilityInSidePanel();

  CHECK(browser_->profile());
  companion::UpdateCompanionDefaultPinnedToToolbarState(browser_->profile());
}

BROWSER_USER_DATA_KEY_IMPL(SearchCompanionSidePanelCoordinator);
