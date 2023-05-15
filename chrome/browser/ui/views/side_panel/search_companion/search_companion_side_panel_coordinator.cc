// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"

SearchCompanionSidePanelCoordinator::SearchCompanionSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<SearchCompanionSidePanelCoordinator>(*browser),
      browser_(browser),
      // TODO(b/269331995): Localize menu item label.
      name_(l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMPANION_TITLE)),
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      icon_(vector_icons::kGoogleGLogoIcon) {
#else
      icon_(vector_icons::kSearchIcon) {
#endif
  if (auto* template_url_service =
          TemplateURLServiceFactory::GetForProfile(browser->profile())) {
    template_url_service_observation_.Observe(template_url_service);
  }
  // Only start observing tab changes if google is the default search provider.
  dsp_is_google_ = search::DefaultSearchProviderIsGoogle(browser_->profile());
  if (dsp_is_google_) {
    browser_->tab_strip_model()->AddObserver(this);
    CreateAndRegisterEntriesForExistingWebContents(browser_->tab_strip_model());
  }
}

SearchCompanionSidePanelCoordinator::~SearchCompanionSidePanelCoordinator() =
    default;

// static
bool SearchCompanionSidePanelCoordinator::IsSupported(Profile* profile,
                                                      bool include_dsp_check) {
  return !profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
         (!include_dsp_check || search::DefaultSearchProviderIsGoogle(profile));
}

bool SearchCompanionSidePanelCoordinator::Show(
    SidePanelOpenTrigger side_panel_open_trigger) {
  auto* browser_view = GetBrowserView();
  if (!browser_view) {
    return false;
  }

  if (auto* side_panel_coordinator = browser_view->side_panel_coordinator()) {
    side_panel_coordinator->Show(SidePanelEntry::Id::kSearchCompanion,
                                 side_panel_open_trigger);
  }

  return true;
}

BrowserView* SearchCompanionSidePanelCoordinator::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

std::u16string
SearchCompanionSidePanelCoordinator::GetTooltipForToolbarButton() {
  return l10n_util::GetStringUTF16(IDS_SIDE_PANEL_COMPANION_TOOLBAR_TOOLTIP);
}

void SearchCompanionSidePanelCoordinator::NotifyCompanionOfSidePanelOpenTrigger(
    absl::optional<SidePanelOpenTrigger> side_panel_open_trigger) {
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
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view) {
    return;
  }
  SidePanelToolbarContainer* container =
      browser_view->toolbar()->side_panel_container();
  bool dsp_was_google = dsp_is_google_;
  dsp_is_google_ = search::DefaultSearchProviderIsGoogle(browser_->profile());

  // Update existence of companion entry points based on changes to the default
  // search provider.
  if (dsp_is_google_ && !dsp_was_google) {
    container->AddPinnedEntryButtonFor(SidePanelEntry::Id::kSearchCompanion,
                                       name(), icon());
    browser_->tab_strip_model()->AddObserver(this);
    CreateAndRegisterEntriesForExistingWebContents(browser_->tab_strip_model());
  } else if (!dsp_is_google_ && dsp_was_google) {
    container->RemovePinnedEntryButtonFor(SidePanelEntry::Id::kSearchCompanion);
    browser_->tab_strip_model()->RemoveObserver(this);
    DeregisterEntriesForExistingWebContents(browser_->tab_strip_model());
  }
}

void SearchCompanionSidePanelCoordinator::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SearchCompanionSidePanelCoordinator);
