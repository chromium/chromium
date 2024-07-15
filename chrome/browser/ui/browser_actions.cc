// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include <optional>
#include <string>

#include "base/check_op.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/performance_manager/public/features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/text_utils.h"
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
      .SetText(BrowserActions::GetCleanTitleAndTooltipText(
          l10n_util::GetStringUTF16(title_id)))
      .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
          l10n_util::GetStringUTF16(tooltip_id)))
      .SetImage(ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon))
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
  return actions::ActionItem::Builder(CreateToggleSidePanelActionCallback(
                                          SidePanelEntryKey(id), browser))
      .SetActionId(action_id)
      .SetText(l10n_util::GetStringUTF16(title_id))
      .SetTooltipText(l10n_util::GetStringUTF16(tooltip_id))
      .SetImage(ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon))
      .SetProperty(actions::kActionItemPinnableKey, is_pinnable);
}
}  // namespace

BrowserActions::BrowserActions(Browser& browser) : browser_(browser) {
}

BrowserActions::~BrowserActions() {
  // Extract the unique ptr and destruct it after the raw_ptr to avoid a
  // dangling pointer scenario.
  std::unique_ptr<actions::ActionItem> owned_root_action_item =
      actions::ActionManager::Get().RemoveAction(root_action_item_);
  root_action_item_ = nullptr;
}

// static
std::u16string BrowserActions::GetCleanTitleAndTooltipText(
    std::u16string string) {
  const std::u16string ellipsis_unicode = u"\u2026";
  const std::u16string ellipsis_text = u"...";

  auto remove_ellipsis = [&string](const std::u16string ellipsis) {
    size_t ellipsis_pos = string.find(ellipsis);
    if (ellipsis_pos != std::u16string::npos) {
      string.erase(ellipsis_pos);
    }
  };
  remove_ellipsis(ellipsis_unicode);
  remove_ellipsis(ellipsis_text);
  return gfx::RemoveAccelerator(string);
}

void BrowserActions::InitializeBrowserActions() {
  Profile* profile = browser_->profile();
  Browser* browser = &(browser_.get());

  actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder()
          .CopyAddressTo(&root_action_item_)
          .AddChildren(
              SidePanelAction(
                  SidePanelEntryId::kBookmarks, IDS_BOOKMARK_MANAGER_TITLE,
                  IDS_BOOKMARK_MANAGER_TITLE, kBookmarksSidePanelRefreshIcon,
                  kActionSidePanelShowBookmarks, browser, true),
              SidePanelAction(SidePanelEntryId::kReadingList,
                              IDS_READ_LATER_TITLE, IDS_READ_LATER_TITLE,
                              kReadingListIcon, kActionSidePanelShowReadingList,
                              browser, true),
              SidePanelAction(SidePanelEntryId::kAboutThisSite,
                              IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                              IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                              PageInfoViewFactory::GetAboutThisSiteVectorIcon(),
                              kActionSidePanelShowAboutThisSite, browser,
                              false),
              SidePanelAction(SidePanelEntryId::kCustomizeChrome,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              vector_icons::kEditChromeRefreshIcon,
                              kActionSidePanelShowCustomizeChrome, browser,
                              false),
              SidePanelAction(SidePanelEntryId::kShoppingInsights,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              vector_icons::kShoppingBagIcon,
                              kActionSidePanelShowShoppingInsights, browser,
                              false))
          .Build());

  if (side_panel::history_clusters::
          IsHistoryClustersSidePanelSupportedForProfile(profile)) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kHistoryClusters, IDS_HISTORY_TITLE,
                        IDS_HISTORY_CLUSTERS_SHOW_SIDE_PANEL,
                        vector_icons::kHistoryChromeRefreshIcon,
                        kActionSidePanelShowHistoryCluster, browser, true)
            .Build());
  }

  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kReadAnything, IDS_READING_MODE_TITLE,
                      IDS_READING_MODE_TITLE, kMenuBookChromeRefreshIcon,
                      kActionSidePanelShowReadAnything, browser, true)
          .Build());

  if (base::FeatureList::IsEnabled(
          performance_manager::features::kPerformanceControlsSidePanel)) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kPerformance, IDS_SHOW_PERFORMANCE,
                        IDS_SHOW_PERFORMANCE, kMemorySaverIcon,
                        kActionSidePanelShowPerformance, browser, true)
            .Build());
  }

  if (lens::features::IsLensOverlayEnabled()) {
    actions::ActionItem::InvokeActionCallback callback = base::BindRepeating(
        [](base::WeakPtr<Browser> browser, actions::ActionItem* item,
           actions::ActionInvocationContext context) {
          if (!browser) {
            return;
          }

          LensOverlayController* controller = browser->GetActiveTabInterface()
                                                  ->GetTabFeatures()
                                                  ->lens_overlay_controller();

          // Toggle the Lens overlay. There's no need to show or hide the side
          // panel as the overlay controller will handle that.
          if (controller->IsOverlayShowing()) {
            controller->CloseUIAsync(
                lens::LensOverlayDismissalSource::kToolbar);
          } else {
            controller->ShowUI(lens::LensOverlayInvocationSource::kToolbar);
          }
        },
        browser->AsWeakPtr());
    const gfx::VectorIcon& icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        vector_icons::kGoogleLensMonochromeLogoIcon;
#else
        vector_icons::kSearchIcon;
#endif
    root_action_item_->AddChild(
        actions::ActionItem::Builder(callback)
            .SetActionId(kActionSidePanelShowLensOverlayResults)
            .SetText(l10n_util::GetStringUTF16(IDS_SHOW_LENS_OVERLAY))
            .SetTooltipText(l10n_util::GetStringUTF16(
                IDS_SIDE_PANEL_LENS_OVERLAY_TOOLBAR_TOOLTIP))
            .SetImage(ui::ImageModel::FromVectorIcon(
                icon, ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
            .SetProperty(actions::kActionItemPinnableKey, true)
            .Build());
  } else if (companion::IsCompanionFeatureEnabled()) {
    if (companion::IsSearchInCompanionSidePanelSupportedForProfile(
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
              kActionSidePanelShowSearchCompanion, browser, true)
              .Build());

      companion_action_item->SetVisible(
          companion::IsSearchInCompanionSidePanelSupportedForProfile(
              profile,
              /*include_runtime_checks=*/true));
    }
  }

  // Create the lens action item. The icon and text are set appropriately in the
  // lens side panel coordinator. They have default values here.
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kLens, IDS_LENS_DEFAULT_TITLE,
                      IDS_LENS_DEFAULT_TITLE, vector_icons::kImageSearchIcon,
                      kActionSidePanelShowLens, browser, false)
          .Build());

  //------- Chrome Menu Actions --------//
  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             chrome::NewIncognitoWindow(browser->profile());
                           },
                           base::Unretained(browser)),
                       kActionNewIncognitoWindow, IDS_NEW_INCOGNITO_WINDOW,
                       IDS_NEW_INCOGNITO_WINDOW, kIncognitoRefreshMenuIcon)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             chrome::Print(browser);
                           },
                           base::Unretained(browser)),
                       kActionPrint, IDS_PRINT, IDS_PRINT, kPrintMenuIcon)
          .SetEnabled(chrome::CanPrint(browser))
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
                           base::Unretained(browser)),
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
                             base::Unretained(browser)),
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
                           base::Unretained(browser)),
                       kActionDevTools, IDS_DEV_TOOLS, IDS_DEV_TOOLS,
                       kDeveloperToolsIcon)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                send_tab_to_self::ShowBubble(
                    browser->tab_strip_model()->GetActiveWebContents());
              },
              base::Unretained(browser)),
          kActionSendTabToSelf, IDS_SEND_TAB_TO_SELF, IDS_SEND_TAB_TO_SELF,
          kDevicesChromeRefreshIcon)
          .SetEnabled(chrome::CanSendTabToSelf(browser))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             chrome::ShowTranslateBubble(browser);
                           },
                           base::Unretained(browser)),
                       kActionShowTranslate, IDS_SHOW_TRANSLATE,
                       IDS_TOOLTIP_TRANSLATE, kTranslateIcon)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             chrome::GenerateQRCode(browser);
                           },
                           base::Unretained(browser)),
                       kActionQrCodeGenerator, IDS_APP_MENU_CREATE_QR_CODE,
                       IDS_APP_MENU_CREATE_QR_CODE, kQrCodeChromeRefreshIcon)
          .SetEnabled(false)
          .Build());
}
