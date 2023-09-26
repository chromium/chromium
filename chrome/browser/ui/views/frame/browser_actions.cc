// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_actions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_clusters/core/features.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

actions::ActionItem::ActionItemBuilder SidePanelAction(
    SidePanelEntryId id,
    absl::optional<int> title_id,
    const gfx::VectorIcon* icon,
    actions::ActionId action_id,
    Browser* browser) {
  const int side_panel_icon_size =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);

  actions::ActionItem::ActionItemBuilder builder =
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](SidePanelEntryId id, Browser* browser,
                 actions::ActionItem* item) {
                SidePanelUI::GetSidePanelUIForBrowser(browser)->Show(id);
              },
              id, browser))
          .SetActionId(action_id);

  if (title_id.has_value()) {
    builder.SetText(l10n_util::GetStringUTF16(title_id.value()));
  }

  if (icon) {
    builder.SetImage(ui::ImageModel::FromVectorIcon(*icon, ui::kColorIcon,
                                                    side_panel_icon_size));
  }

  return builder;
}

}  // namespace

BrowserActions::BrowserActions(Browser& browser)
    : action_initialization_subscription_(
          actions::ActionManager::Get().AppendActionItemInitializer(
              base::BindRepeating(&BrowserActions::InitializeBrowserActions,
                                  base::Unretained(this)))),
      browser_(browser) {}

BrowserActions::~BrowserActions() = default;

void BrowserActions::InitializeBrowserActions(actions::ActionManager* manager) {
  const bool rename_journeys =
      base::FeatureList::IsEnabled(history_clusters::kRenameJourneys);

  manager->AddActions(
      actions::ActionItem::Builder()
          .CopyAddressTo(&root_action_item_)
          .AddChildren(
              SidePanelAction(SidePanelEntryId::kBookmarks,
                              IDS_BOOKMARK_MANAGER_TITLE, &omnibox::kStarIcon,
                              kActionSidePanelShowBookmarks, &(browser_.get())),
              SidePanelAction(SidePanelEntryId::kReadingList,
                              IDS_READ_LATER_TITLE, &kReadLaterIcon,
                              kActionSidePanelShowReadingList,
                              &(browser_.get())),
              SidePanelAction(
                  SidePanelEntryId::kHistoryClusters,
                  rename_journeys ? IDS_HISTORY_TITLE
                                  : IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL,
                  rename_journeys ? &kHistoryIcon : &kJourneysIcon,
                  kActionSidePanelShowHistoryCluster, &(browser_.get())),
              SidePanelAction(
                  SidePanelEntryId::kReadAnything, IDS_READING_MODE_TITLE,
                  &kMenuBookChromeRefreshIcon, kActionSidePanelShowReadAnything,
                  &(browser_.get())),
              SidePanelAction(SidePanelEntryId::kUserNote, IDS_USER_NOTE_TITLE,
                              &kNoteOutlineIcon, kActionSidePanelShowUserNote,
                              &(browser_.get())),
              SidePanelAction(SidePanelEntryId::kFeed, IDS_FEED_TITLE,
                              &vector_icons::kFeedIcon,
                              kActionSidePanelShowFeed, &(browser_.get())),
              SidePanelAction(SidePanelEntryId::kPerformance,
                              IDS_SHOW_PERFORMANCE, &kHighEfficiencyIcon,
                              kActionSidePanelShowPerformance,
                              &(browser_.get())),
              SidePanelAction(SidePanelEntryId::kSideSearch, absl::nullopt,
                              nullptr, kActionSidePanelShowSideSearch,
                              &(browser_.get())),
              SidePanelAction(
                  SidePanelEntryId::kAboutThisSite,
                  IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                  &PageInfoViewFactory::GetAboutThisSiteColorVectorIcon(),
                  kActionSidePanelShowAboutThisSite, &(browser_.get())),
              SidePanelAction(SidePanelEntryId::kCustomizeChrome,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              &vector_icons::kEditIcon,
                              kActionSidePanelShowCustomizeChrome,
                              &(browser_.get())),
              SidePanelAction(
                  SidePanelEntryId::kSearchCompanion,
                  IDS_SIDE_PANEL_COMPANION_TITLE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                  &vector_icons::
                      kGoogleSearchCompanionMonochromeLogoChromeRefreshIcon,
#else
                  &vector_icons::kSearchIcon,
#endif
                  kActionSidePanelShowSearchCompanion, &(browser_.get())),
              SidePanelAction(SidePanelEntryId::kShoppingInsights,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              &vector_icons::kShoppingBagIcon,
                              kActionSidePanelShowShoppingInsights,
                              &(browser_.get())))
          .Build());
}
