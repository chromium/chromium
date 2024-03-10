// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_actions.h"

#include "base/check_op.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_clusters/core/features.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/performance_manager/public/features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_notes/user_notes_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

namespace {
actions::ActionItem::ActionItemBuilder ChromeMenuAction(
    actions::ActionItem::InvokeActionCallback callback,
    actions::ActionId action_id,
    int title_id,
    int tooltip_id,
    const gfx::VectorIcon& icon) {
  return actions::ActionItem::Builder(callback)
      .SetActionId(action_id)
      .SetText(l10n_util::GetStringUTF16(title_id))
      .SetTooltipText(l10n_util::GetStringUTF16(tooltip_id))
      .SetImage(ui::ImageModel::FromVectorIcon(
          icon, ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
      .SetProperty(actions::kActionItemPinnableKey, true);
}

actions::ActionItem::ActionItemBuilder SidePanelAction(
    SidePanelEntryId id,
    int title_id,
    int tooltip_id,
    const gfx::VectorIcon& icon,
    actions::ActionId action_id,
    Browser* browser,
    bool is_pinnable) {
  const int side_panel_icon_size =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);

  return actions::ActionItem::Builder(
             SidePanelUtil::CreateToggleSidePanelActionCallback(
                 SidePanelEntryKey(id), browser))
      .SetActionId(action_id)
      .SetText(l10n_util::GetStringUTF16(title_id))
      .SetTooltipText(l10n_util::GetStringUTF16(tooltip_id))
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
  Profile* profile = browser_->profile();

  actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder()
          .CopyAddressTo(&root_action_item_)
          .AddChildren(
              SidePanelAction(
                  SidePanelEntryId::kBookmarks, IDS_BOOKMARK_MANAGER_TITLE,
                  IDS_BOOKMARK_MANAGER_TITLE,
                  features::IsChromeRefresh2023()
                      ? kBookmarksSidePanelRefreshIcon
                      : kBookmarksSidePanelIcon,
                  kActionSidePanelShowBookmarks, &(browser_.get()), true),
              SidePanelAction(SidePanelEntryId::kReadingList,
                              IDS_READ_LATER_TITLE, IDS_READ_LATER_TITLE,
                              kReadingListIcon, kActionSidePanelShowReadingList,
                              &(browser_.get()), true),
              SidePanelAction(SidePanelEntryId::kAboutThisSite,
                              IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                              IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                              PageInfoViewFactory::GetAboutThisSiteVectorIcon(),
                              kActionSidePanelShowAboutThisSite,
                              &(browser_.get()), false),
              SidePanelAction(SidePanelEntryId::kCustomizeChrome,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              vector_icons::kEditChromeRefreshIcon,
                              kActionSidePanelShowCustomizeChrome,
                              &(browser_.get()), false),
              SidePanelAction(SidePanelEntryId::kShoppingInsights,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              vector_icons::kShoppingBagIcon,
                              kActionSidePanelShowShoppingInsights,
                              &(browser_.get()), false))
          .Build());

  if (HistoryClustersSidePanelCoordinator::IsSupported(profile)) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kHistoryClusters, IDS_HISTORY_TITLE,
                        IDS_HISTORY_CLUSTERS_SHOW_SIDE_PANEL,
                        vector_icons::kHistoryChromeRefreshIcon,
                        kActionSidePanelShowHistoryCluster, &(browser_.get()),
                        true)
            .Build());
  }

  if (features::IsReadAnythingEnabled()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kReadAnything, IDS_READING_MODE_TITLE,
                        IDS_READING_MODE_TITLE, kMenuBookChromeRefreshIcon,
                        kActionSidePanelShowReadAnything, &(browser_.get()),
                        true)
            .Build());
  }

  if (user_notes::IsUserNotesEnabled()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kUserNote, IDS_USER_NOTE_TITLE,
                        IDS_USER_NOTE_TITLE, kNoteOutlineIcon,
                        kActionSidePanelShowUserNote, &(browser_.get()), true)
            .Build());
  }

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kPerformanceControlsSidePanel)) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kPerformance, IDS_SHOW_PERFORMANCE,
                        IDS_SHOW_PERFORMANCE, kMemorySaverIcon,
                        kActionSidePanelShowPerformance, &(browser_.get()),
                        true)
            .Build());
  }

  if (companion::IsCompanionFeatureEnabled()) {
    if (SearchCompanionSidePanelCoordinator::IsSupported(
            profile,
            /*include_runtime_checks=*/false)) {
      actions::ActionItem* companion_action_item = root_action_item_->AddChild(
          SidePanelAction(
              SidePanelEntryId::kSearchCompanion,
              IDS_SIDE_PANEL_COMPANION_TITLE,
              IDS_SIDE_PANEL_COMPANION_TOOLBAR_TOOLTIP,
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
              profile,
              /*include_runtime_checks=*/true));
    }
  }

  if (lens::features::IsLensOverlayEnabled()) {
    // TODO(b/328295358): Change title and icon when available.
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kLensOverlayResults,
                        IDS_SIDE_PANEL_COMPANION_TITLE,
                        IDS_SIDE_PANEL_COMPANION_TOOLBAR_TOOLTIP,
                        vector_icons::kSearchIcon,
                        kActionSidePanelShowLensOverlayResults,
                        &(browser_.get()), /*is_pinnable=*/true)
            .Build());
  }

  // Create the lens action item. The icon and text are set appropriately in the
  // lens side panel coordinator. They have default values here.
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kLens, IDS_LENS_DEFAULT_TITLE,
                      IDS_LENS_DEFAULT_TITLE, vector_icons::kImageSearchIcon,
                      kActionSidePanelShowLens, &(browser_.get()), false)
          .Build());

  //------- Chrome Menu Actions --------//
  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             chrome::NewIncognitoWindow(browser->profile());
                           },
                           base::Unretained(&(browser_.get()))),
                       kActionNewIncognitoWindow, IDS_NEW_INCOGNITO_WINDOW,
                       IDS_NEW_INCOGNITO_WINDOW, kIncognitoRefreshMenuIcon)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             chrome::Print(browser);
                           },
                           base::Unretained(&(browser_.get()))),
                       kActionPrint, IDS_PRINT, IDS_PRINT, kPrintMenuIcon)
          .SetEnabled(chrome::CanPrint(&(browser_.get())))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             if (browser->profile()->IsIncognitoProfile()) {
                               chrome::ShowIncognitoClearBrowsingDataDialog(
                                   browser->GetBrowserForOpeningWebUi());
                             } else {
                               chrome::ShowClearBrowsingDataDialog(
                                   browser->GetBrowserForOpeningWebUi());
                             }
                           },
                           base::Unretained(&(browser_.get()))),
                       kActionClearBrowsingData, IDS_CLEAR_BROWSING_DATA,
                       IDS_CLEAR_BROWSING_DATA, kTrashCanRefreshIcon)
          .SetEnabled(
              profile->IsIncognitoProfile() ||
              (!profile->IsGuestSession() && !profile->IsSystemProfile()))
          .Build());

  if (chrome::CanOpenTaskManager()) {
    root_action_item_->AddChild(
        ChromeMenuAction(base::BindRepeating(
                             [](Browser* browser, actions::ActionItem* item,
                                actions::ActionInvocationContext context) {
                               chrome::OpenTaskManager(browser);
                             },
                             base::Unretained(&(browser_.get()))),
                         kActionTaskManager, IDS_TASK_MANAGER, IDS_TASK_MANAGER,
                         kTaskManagerIcon)
            .Build());
  }

  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             chrome::ToggleDevToolsWindow(
                                 browser, DevToolsToggleAction::Show(),
                                 DevToolsOpenedByAction::kPinnedToolbarButton);
                           },
                           base::Unretained(&(browser_.get()))),
                       kActionDevTools, IDS_DEV_TOOLS, IDS_DEV_TOOLS,
                       kDeveloperToolsIcon)
          .Build());
}
