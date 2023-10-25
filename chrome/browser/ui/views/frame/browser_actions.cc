// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_actions.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "components/feed/feed_feature_list.h"
#include "components/history_clusters/core/features.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/performance_manager/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_notes/user_notes_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace {

actions::ActionItem::ActionItemBuilder SidePanelAction(
    SidePanelEntryId id,
    int title_id,
    const gfx::VectorIcon& icon,
    actions::ActionId action_id,
    Browser* browser,
    bool is_pinnable) {
  const int side_panel_icon_size =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);

  return actions::ActionItem::Builder(
             base::BindRepeating(
                 [](SidePanelEntryId id, Browser* browser,
                    actions::ActionItem* item,
                    actions::ActionInvocationContext context) {
                   SidePanelUI::GetSidePanelUIForBrowser(browser)->Show(id);
                 },
                 id, browser))
      .SetActionId(action_id)
      .SetText(l10n_util::GetStringUTF16(title_id))
      .SetImage(ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon,
                                               side_panel_icon_size))
      .SetProperty(actions::kActionItemPinnableKey, is_pinnable);
}
}  // namespace

const int BrowserActions::kUserDataKey;

BrowserActions::BrowserActions(Browser& browser) : browser_(browser) {
  BrowserActions::InitializeBrowserActions();
}

BrowserActions::~BrowserActions() {
  // Extract the unique ptr and destruct it after the raw_ptr to avoid a
  // dangling pointer scenario.
  std::unique_ptr<actions::ActionItem> owned_root_action_item =
      actions::ActionManager::Get().RemoveAction(root_action_item_);
  root_action_item_ = nullptr;
}

// static
BrowserActions* BrowserActions::FromBrowser(Browser* browser) {
  return static_cast<BrowserActions*>(
      browser->GetUserData(BrowserActions::UserDataKey()));
}

void BrowserActions::InitializeBrowserActions() {
  const bool rename_journeys =
      base::FeatureList::IsEnabled(history_clusters::kRenameJourneys);

  actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder()
          .CopyAddressTo(&root_action_item_)
          .AddChildren(
              SidePanelAction(SidePanelEntryId::kBookmarks,
                              IDS_BOOKMARK_MANAGER_TITLE, omnibox::kStarIcon,
                              kActionSidePanelShowBookmarks, &(browser_.get()),
                              true),
              SidePanelAction(SidePanelEntryId::kReadingList,
                              IDS_READ_LATER_TITLE, kReadLaterIcon,
                              kActionSidePanelShowReadingList,
                              &(browser_.get()), true),
              SidePanelAction(
                  SidePanelEntryId::kAboutThisSite,
                  IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                  PageInfoViewFactory::GetAboutThisSiteColorVectorIcon(),
                  kActionSidePanelShowAboutThisSite, &(browser_.get()), false),
              SidePanelAction(SidePanelEntryId::kCustomizeChrome,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              vector_icons::kEditIcon,
                              kActionSidePanelShowCustomizeChrome,
                              &(browser_.get()), false),
              SidePanelAction(SidePanelEntryId::kShoppingInsights,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              vector_icons::kShoppingBagIcon,
                              kActionSidePanelShowShoppingInsights,
                              &(browser_.get()), false))
          .Build());

  if (HistoryClustersSidePanelCoordinator::IsSupported(browser_->profile())) {
    root_action_item_->AddChild(
        SidePanelAction(
            SidePanelEntryId::kHistoryClusters,
            rename_journeys ? IDS_HISTORY_TITLE
                            : IDS_HISTORY_CLUSTERS_JOURNEYS_TAB_LABEL,
            rename_journeys ? kHistoryIcon : kJourneysIcon,
            kActionSidePanelShowHistoryCluster, &(browser_.get()), true)
            .Build());
  }

  if (features::IsReadAnythingEnabled()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kReadAnything, IDS_READING_MODE_TITLE,
                        kMenuBookChromeRefreshIcon,
                        kActionSidePanelShowReadAnything, &(browser_.get()),
                        true)
            .Build());
  }

  if (user_notes::IsUserNotesEnabled()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kUserNote, IDS_USER_NOTE_TITLE,
                        kNoteOutlineIcon, kActionSidePanelShowUserNote,
                        &(browser_.get()), true)
            .Build());
  }

  if (base::FeatureList::IsEnabled(feed::kWebUiFeed)) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kFeed, IDS_FEED_TITLE,
                        vector_icons::kFeedIcon, kActionSidePanelShowFeed,
                        &(browser_.get()), true)
            .Build());
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kPerformanceControlsSidePanel)) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kPerformance, IDS_SHOW_PERFORMANCE,
                        kHighEfficiencyIcon, kActionSidePanelShowPerformance,
                        &(browser_.get()), true)
            .Build());
  }

  if (companion::IsCompanionFeatureEnabled()) {
    if (SearchCompanionSidePanelCoordinator::IsSupported(
            browser_->profile(),
            /*include_runtime_checks=*/false)) {
      actions::ActionItem* companion_action_item =

          root_action_item_->AddChild(
              SidePanelAction(
                  SidePanelEntryId::kSearchCompanion,
                  IDS_SIDE_PANEL_COMPANION_TITLE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                  vector_icons::
                      kGoogleSearchCompanionMonochromeLogoChromeRefreshIcon,
#else
                  vector_icons::kSearchIcon,
#endif
                  kActionSidePanelShowSearchCompanion, &(browser_.get()), true)
                  .Build());

      companion_action_item->SetVisible(
          SearchCompanionSidePanelCoordinator::IsSupported(
              browser_->profile(),
              /*include_runtime_checks=*/true));
    }
  }
}
