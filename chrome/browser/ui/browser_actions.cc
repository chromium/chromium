// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/autofill/address_bubbles_icon_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_action_prefs_listener.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_string_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_util.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/discounts_page_action_view_controller.h"
#include "chrome/browser/ui/views/commerce/product_specifications_page_action_view_controller.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/commerce/core/metrics/discounts_metric_collector.h"
#include "components/lens/lens_features.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view_class_properties.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#endif

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
      .SetProperty(actions::kActionItemPinnableKey,
                   std::underlying_type_t<actions::ActionPinnableState>(
                       actions::ActionPinnableState::kPinnable));
}

actions::StatefulImageActionItem::StatefulImageActionItemBuilder
StatefulChromeMenuAction(actions::ActionItem::InvokeActionCallback callback,
                         actions::ActionId action_id,
                         int title_id,
                         int tooltip_id,
                         const gfx::VectorIcon& icon) {
  ui::ImageModel image = ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon);
  return actions::StatefulImageActionItem::Builder(callback)
      .SetActionId(action_id)
      .SetText(BrowserActions::GetCleanTitleAndTooltipText(
          l10n_util::GetStringUTF16(title_id)))
      .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
          l10n_util::GetStringUTF16(tooltip_id)))
      .SetImage(image)
      .SetStatefulImage(image)
      .SetProperty(actions::kActionItemPinnableKey,
                   std::underlying_type_t<actions::ActionPinnableState>(
                       actions::ActionPinnableState::kPinnable));
}

actions::ActionItem::ActionItemBuilder SidePanelAction(
    SidePanelEntryId id,
    int title_id,
    int tooltip_id,
    const gfx::VectorIcon& icon,
    actions::ActionId action_id,
    BrowserWindowInterface* bwi,
    bool is_pinnable) {
  auto pinnable_state =
      is_pinnable ? std::underlying_type_t<actions::ActionPinnableState>(
                        actions::ActionPinnableState::kPinnable)
                  : std::underlying_type_t<actions::ActionPinnableState>(
                        actions::ActionPinnableState::kNotPinnable);
  return actions::ActionItem::Builder(
             CreateToggleSidePanelActionCallback(SidePanelEntryKey(id), bwi))
      .SetActionId(action_id)
      .SetText(l10n_util::GetStringUTF16(title_id))
      .SetTooltipText(l10n_util::GetStringUTF16(tooltip_id))
      .SetImage(ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon))
      .SetProperty(actions::kActionItemPinnableKey, pinnable_state);
}
}  // namespace

BrowserActions::BrowserActions(BrowserWindowInterface* bwi)
    : bwi_(CHECK_DEREF(bwi)), profile_(CHECK_DEREF(bwi->GetProfile())) {}

BrowserActions::~BrowserActions() {
  browser_action_prefs_listener_.reset();

  // Extract the unique ptr and destruct it after the raw_ptr to avoid a
  // dangling pointer scenario.
  std::unique_ptr<actions::ActionItem> owned_root_action_item =
      actions::ActionManager::Get().RemoveAction(root_action_item_);
  root_action_item_ = nullptr;
}

// static
std::u16string BrowserActions::GetCleanTitleAndTooltipText(
    std::u16string string) {
  static constexpr std::u16string_view kEllipsisUnicode{u"\u2026"};
  static constexpr std::u16string_view kEllipsisText{u"..."};

  const auto remove_ellipsis = [&string](const std::u16string_view ellipsis) {
    const size_t ellipsis_pos = string.find(ellipsis);
    if (ellipsis_pos != std::u16string::npos) {
      string.erase(ellipsis_pos);
    }
  };
  remove_ellipsis(kEllipsisUnicode);
  remove_ellipsis(kEllipsisText);
  return gfx::RemoveAccelerator(string);
}

void BrowserActions::InitializeBrowserActions() {
  Profile* const profile = base::to_address(profile_);
  Browser* const browser = bwi_->GetBrowserForMigrationOnly();
  BrowserWindowInterface* const bwi = base::to_address(bwi_);
  const bool is_guest_session = profile_->IsGuestSession();

  actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder()
          .CopyAddressTo(&root_action_item_)
          .AddChildren(
              SidePanelAction(
                  SidePanelEntryId::kBookmarks, IDS_BOOKMARK_MANAGER_TITLE,
                  IDS_BOOKMARK_MANAGER_TITLE, kBookmarksSidePanelRefreshIcon,
                  kActionSidePanelShowBookmarks, bwi, true),
              SidePanelAction(SidePanelEntryId::kReadingList,
                              IDS_READ_LATER_TITLE, IDS_READ_LATER_TITLE,
                              kReadingListIcon, kActionSidePanelShowReadingList,
                              bwi, true),
              SidePanelAction(SidePanelEntryId::kAboutThisSite,
                              IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                              IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                              PageInfoViewFactory::GetAboutThisSiteVectorIcon(),
                              kActionSidePanelShowAboutThisSite, bwi, false),
              SidePanelAction(SidePanelEntryId::kCustomizeChrome,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                              vector_icons::kEditChromeRefreshIcon,
                              kActionSidePanelShowCustomizeChrome, bwi, false),
              SidePanelAction(SidePanelEntryId::kShoppingInsights,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                              vector_icons::kShoppingBagIcon,
                              kActionSidePanelShowShoppingInsights, bwi, false),
              SidePanelAction(SidePanelEntryId::kMerchantTrust,
                              IDS_MERCHANT_TRUST_SIDE_PANEL_TITLE,
                              IDS_MERCHANT_TRUST_SIDE_PANEL_TITLE,
                              vector_icons::kStorefrontIcon,
                              kActionSidePanelShowMerchantTrust, bwi, false))
          .Build());

  if (side_panel::history_clusters::
          IsHistoryClustersSidePanelSupportedForProfile(profile) &&
      !HistorySidePanelCoordinator::IsSupported()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kHistoryClusters, IDS_HISTORY_TITLE,
                        IDS_HISTORY_CLUSTERS_SHOW_SIDE_PANEL,
                        vector_icons::kHistoryChromeRefreshIcon,
                        kActionSidePanelShowHistoryCluster, bwi, true)
            .Build());
  }

  if (HistorySidePanelCoordinator::IsSupported()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kHistory, IDS_HISTORY_TITLE,
                        IDS_HISTORY_SHOW_SIDE_PANEL,
                        vector_icons::kHistoryChromeRefreshIcon,
                        kActionSidePanelShowHistory, bwi, true)
            .Build());
  }

  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kReadAnything, IDS_READING_MODE_TITLE,
                      IDS_READING_MODE_TITLE, kMenuBookChromeRefreshIcon,
                      kActionSidePanelShowReadAnything, bwi, true)
          .Build());

  if (lens::features::IsLensOverlayEnabled()) {
    actions::ActionItem::InvokeActionCallback callback = base::BindRepeating(
        [](base::WeakPtr<BrowserWindowInterface> bwi, actions::ActionItem* item,
           actions::ActionInvocationContext context) {
          if (!bwi) {
            return;
          }
          lens::LensOverlayEntryPointController::InvokeAction(
              bwi->GetActiveTabInterface(), context);
        },
        bwi->GetWeakPtr());
    const gfx::VectorIcon& icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        vector_icons::kGoogleLensMonochromeLogoIcon;
#else
        vector_icons::kSearchChromeRefreshIcon;
#endif
    root_action_item_->AddChild(
        actions::ActionItem::Builder(callback)
            .SetActionId(kActionSidePanelShowLensOverlayResults)
            .SetText(l10n_util::GetStringUTF16(
                lens::GetLensOverlayEntrypointLabelAltIds(
                    IDS_SHOW_LENS_OVERLAY)))
            .SetTooltipText(l10n_util::GetStringUTF16(
                lens::GetLensOverlayEntrypointLabelAltIds(
                    IDS_SIDE_PANEL_LENS_OVERLAY_TOOLBAR_TOOLTIP)))
            .SetImage(ui::ImageModel::FromVectorIcon(
                icon, ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
            .SetProperty(actions::kActionItemPinnableKey,
                         std::underlying_type_t<actions::ActionPinnableState>(
                             actions::ActionPinnableState::kPinnable))
            .Build());
  }

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowOffersAndRewardsForPage(bwi);
              },
              bwi))
          .SetActionId(kActionOffersAndRewardsForPage)
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT))
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT))
          .SetImage(ui::ImageModel::FromVectorIcon(
              kLocalOfferFlippedRefreshIcon, ui::kColorIcon,
              ui::SimpleMenuModel::kDefaultIconSize))
          .Build());

  // Create the lens action item. The icon and text are set appropriately in the
  // lens side panel coordinator. They have default values here.
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kLens, IDS_LENS_DEFAULT_TITLE,
                      IDS_LENS_DEFAULT_TITLE, vector_icons::kImageSearchIcon,
                      kActionSidePanelShowLens, bwi, false)
          .Build());

  // TODO(crbug.com/435220196): Ideally this action would have
  // MemorySaverBubbleController passed in as a dependency directly.
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* bubble_controller =
                    bwi->GetFeatures().memory_saver_bubble_controller();
                bubble_controller->InvokeAction(bwi, item);
              },
              bwi))
          .SetActionId(kActionShowMemorySaverChip)
          .SetText(l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_CHIP_LABEL))
          .SetTooltipText(
              l10n_util::GetStringUTF16(IDS_MEMORY_SAVER_CHIP_ACCNAME))
          .SetImage(ui::ImageModel::FromVectorIcon(
              kPerformanceSpeedometerIcon, ui::kColorIcon,
              ui::SimpleMenuModel::kDefaultIconSize))
          .SetEnabled(true)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetActiveTabInterface()
                    ->GetTabFeatures()
                    ->zoom_view_controller()
                    ->UpdateBubbleVisibility(
                        /*prefer_to_show_bubble=*/true,
                        /*from_user_gesture=*/true);
              },
              bwi))
          .SetActionId(kActionZoomNormal)
          .SetText(l10n_util::GetStringUTF16(IDS_ZOOM_NORMAL))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_ZOOM))
          .SetImage(ui::ImageModel::FromVectorIcon(kZoomInIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* tab_helper = bwi->GetActiveTabInterface()
                                       ->GetTabFeatures()
                                       ->commerce_ui_tab_helper();
                CHECK(tab_helper);

                tab_helper->OnPriceInsightsIconClicked();
              },
              bwi))
          .SetActionId(kActionCommercePriceInsights)
          // The tooltip text is used as a default text. The
          // PriceInsightsPageActionViewController will override it based on its
          // state.
          .SetText(l10n_util::GetStringUTF16(
              IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT))
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT))
          .SetImage(ui::ImageModel::FromVectorIcon(
              vector_icons::kShoppingBagRefreshIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* tab_features =
                    bwi->GetActiveTabInterface()->GetTabFeatures();
                CHECK(tab_features);

                tab_features->commerce_discounts_page_action_view_controller()
                    ->MaybeShowBubble(/*from_user=*/true);

                auto* commerce_ui_tab_helper =
                    tab_features->commerce_ui_tab_helper();
                CHECK(commerce_ui_tab_helper);

                commerce::metrics::DiscountsMetricCollector::
                    RecordDiscountsPageActionIconClicked(
                        commerce_ui_tab_helper->IsPageActionIconExpanded(
                            PageActionIconType::kDiscounts),
                        commerce_ui_tab_helper->GetDiscounts());
              },
              bwi))
          .SetActionId(kActionCommerceDiscounts)
          .SetText(l10n_util::GetStringUTF16(IDS_DISCOUNT_ICON_EXPANDED_TEXT))
          .SetTooltipText(
              l10n_util::GetStringUTF16(IDS_DISCOUNT_ICON_EXPANDED_TEXT))
          .SetImage(
              ui::ImageModel::FromVectorIcon(vector_icons::kShoppingmodeIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* tab_features =
                    browser->GetActiveTabInterface()->GetTabFeatures();
                CHECK(tab_features);

                tab_features
                    ->commerce_product_specifications_page_action_view_controller()
                    ->ShowConfirmationToast();
              },
              base::Unretained(browser)))
          .SetActionId(kActionCommerceProductSpecifications)
          .SetText(
              l10n_util::GetStringUTF16(IDS_COMPARE_PAGE_ACTION_ADD_DEFAULT))
          .SetTooltipText(
              l10n_util::GetStringUTF16(IDS_COMPARE_PAGE_ACTION_ADD_DEFAULT))
          .SetImage(ui::ImageModel::FromVectorIcon(
              omnibox::kProductSpecificationsAddIcon))
          .Build());

  // Clicking the Mandatory Reauth page action is a no-op. This is because the
  // icon is always shown with a dialog bubble. The expected behavior is to
  // simply close this bubble, which happens automatically due to focus change
  // when the user clicks the icon. Therefore, a `base::DoNothing()` callback is
  // used.
  root_action_item_->AddChild(
      actions::ActionItem::Builder(base::DoNothing())
          .SetActionId(kActionAutofillMandatoryReauth)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_MANDATORY_REAUTH_ICON_TOOLTIP))
          .SetImage(
              ui::ImageModel::FromVectorIcon(kCreditCardChromeRefreshIcon))
          .Build());

  //------- Chrome Menu Actions --------//
  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             CHECK(IncognitoModePrefs::IsIncognitoAllowed(
                                 browser->profile()));
                             chrome::NewIncognitoWindow(browser->profile());
                           },
                           base::Unretained(browser)),
                       kActionNewIncognitoWindow, IDS_NEW_INCOGNITO_WINDOW,
                       IDS_NEW_INCOGNITO_WINDOW, kIncognitoRefreshMenuIcon)
          .SetEnabled(IncognitoModePrefs::IsIncognitoAllowed(profile))
          .Build());

  if (features::HasTabSearchToolbarButton()) {
    root_action_item_->AddChild(
        ChromeMenuAction(base::BindRepeating(
                             [](Browser* browser, actions::ActionItem* item,
                                actions::ActionInvocationContext context) {
                               chrome::ShowTabSearch(browser);
                             },
                             base::Unretained(browser)),
                         kActionTabSearch, IDS_TAB_SEARCH_MENU,
                         IDS_TAB_SEARCH_MENU, vector_icons::kTabSearchIcon)
            .Build());
  }

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

  if (send_tab_to_self::SendTabToSelfToolbarIconController::CanShowOnBrowser(
          browser)) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](Browser* browser, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* bubble_controller =
                      browser->browser_window_features()
                          ->send_tab_to_self_toolbar_bubble_controller();
                  if (bubble_controller->IsBubbleShowing()) {
                    bubble_controller->HideBubble();
                  } else {
                    send_tab_to_self::ShowBubble(
                        browser->tab_strip_model()->GetActiveWebContents());
                  }
                },
                base::Unretained(browser)),
            kActionSendTabToSelf, IDS_SEND_TAB_TO_SELF, IDS_SEND_TAB_TO_SELF,
            kDevicesChromeRefreshIcon)
            .SetEnabled(chrome::CanSendTabToSelf(browser))
            .SetVisible(
                !sharing_hub::SharingIsDisabledByPolicy(browser->profile()))
            .Build());
  }

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
          .SetVisible(
              !sharing_hub::SharingIsDisabledByPolicy(browser->profile()))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* controller = autofill::AddressBubblesIconController::Get(
                    browser->tab_strip_model()->GetActiveWebContents());
                if (controller && controller->GetBubbleView()) {
                  controller->GetBubbleView()->Hide();
                } else {
                  chrome::ShowAddresses(browser);
                }
              },
              base::Unretained(browser)),
          kActionShowAddressesBubbleOrPage,
          IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
          IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
          vector_icons::kLocationOnChromeRefreshIcon)
          .SetEnabled(!is_guest_session)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto hide_bubble = [&browser](int command_id) -> bool {
                  auto* controller = autofill::SavePaymentIconController::Get(
                      browser->tab_strip_model()->GetActiveWebContents(),
                      command_id);
                  if (controller && controller->GetPaymentBubbleView()) {
                    controller->GetPaymentBubbleView()->Hide();
                    return true;
                  }
                  return false;
                };
                const bool bubble_hidden =
                    hide_bubble(IDC_SAVE_CREDIT_CARD_FOR_PAGE) ||
                    hide_bubble(IDC_SAVE_IBAN_FOR_PAGE);
                if (!bubble_hidden) {
                  chrome::ShowPaymentMethods(browser);
                }
              },
              base::Unretained(browser)),
          kActionShowPaymentsBubbleOrPage, IDS_PAYMENT_METHOD_SUBMENU_OPTION,
          IDS_PAYMENT_METHOD_SUBMENU_OPTION, kCreditCardChromeRefreshIcon)
          .SetEnabled(!is_guest_session)
          .Build());

  if (IsChromeLabsEnabled() &&
      !web_app::AppBrowserController::IsWebApp(browser)) {
    root_action_item_->AddChild(
        ChromeMenuAction(base::BindRepeating(
                             [](Browser* browser, actions::ActionItem* item,
                                actions::ActionInvocationContext context) {
                               browser->window()->ShowChromeLabs();
                             },
                             base::Unretained(browser)),
                         kActionShowChromeLabs, IDS_CHROMELABS, IDS_CHROMELABS,
                         kScienceIcon)
            .SetVisible(ShouldShowChromeLabsUI(browser->profile()))
            .Build());
  }

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (PasswordsModelDelegateFromWebContents(
                        browser->tab_strip_model()->GetActiveWebContents())
                        ->GetState() == password_manager::ui::INACTIVE_STATE) {
                  chrome::ShowPasswordManager(browser);
                } else {
                  content::WebContents* web_contents =
                      browser->tab_strip_model()->GetActiveWebContents();
                  auto* controller =
                      ManagePasswordsUIController::FromWebContents(
                          web_contents);
                  if (controller->IsShowingBubble()) {
                    controller->HidePasswordBubble();
                  } else {
                    chrome::ManagePasswordsForPage(browser);
                  }
                }
              },
              base::Unretained(browser)),
          kActionShowPasswordsBubbleOrPage, IDS_VIEW_PASSWORDS,
          IDS_VIEW_PASSWORDS, vector_icons::kPasswordManagerIcon)
          .SetEnabled(!is_guest_session)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                content::WebContents* web_contents =
                    browser->tab_strip_model()->GetActiveWebContents();
                const GURL& url = chrome::GetURLToBookmark(web_contents);
                IntentPickerTabHelper* intent_picker_tab_helper =
                    IntentPickerTabHelper::FromWebContents(web_contents);
                CHECK(intent_picker_tab_helper);
                intent_picker_tab_helper->ShowIntentPickerBubbleOrLaunchApp(
                    url);
              },
              base::Unretained(browser)))
          .SetActionId(kActionShowIntentPicker)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_TOOLTIP_INTENT_PICKER_ICON)))
          .SetImage(ui::ImageModel::FromVectorIcon(kOpenInNewChromeRefreshIcon,
                                                   ui::kColorIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                // Show the File System Access bubble if applicable for
                // the current page state.
                FileSystemAccessBubbleController::Show(browser);
              },
              base::Unretained(browser)))
          .SetActionId(kActionShowFileSystemAccess)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(
                  IDS_FILE_SYSTEM_ACCESS_WRITE_USAGE_TOOLTIP)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(
                  IDS_FILE_SYSTEM_ACCESS_WRITE_USAGE_TOOLTIP)))
          .SetImage(ui::ImageModel::FromVectorIcon(kFileSaveChromeRefreshIcon,
                                                   ui::kColorIcon))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CopyURL(
                    browser,
                    browser->tab_strip_model()->GetActiveWebContents());
              },
              base::Unretained(browser)),
          kActionCopyUrl, IDS_APP_MENU_COPY_LINK, IDS_APP_MENU_COPY_LINK,
          kLinkChromeRefreshIcon)
          .SetEnabled(chrome::CanCopyUrl(browser))
          .SetVisible(
              !sharing_hub::SharingIsDisabledByPolicy(browser->profile()))
          .Build());

  actions::ActionItem* media_router_action;
  root_action_item_->AddChild(
      StatefulChromeMenuAction(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                // TODO(crbug.com/356468503): Figure out how to capture
                // action invocation location.
                auto* cast_browser_controller =
                    browser->browser_window_features()
                        ->cast_browser_controller();
                if (cast_browser_controller) {
                  cast_browser_controller->ToggleDialog();
                }
              },
              base::Unretained(browser)),
          kActionRouteMedia, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE,
          IDS_MEDIA_ROUTER_ICON_TOOLTIP_TEXT, kCastChromeRefreshIcon)
          .SetEnabled(chrome::CanRouteMedia(browser))
          .CopyAddressTo(&media_router_action)
          .Build());
  CastToolbarButtonUtil::AddCastChildActions(media_router_action, browser);

#if !BUILDFLAG(IS_CHROMEOS)
  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](Browser* browser, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             browser->GetFeatures()
                                 .download_toolbar_ui_controller()
                                 ->InvokeUI();
                           },
                           base::Unretained(browser)),
                       kActionShowDownloads, IDS_SHOW_DOWNLOADS,
                       IDS_TOOLTIP_DOWNLOAD_ICON,
                       kDownloadToolbarButtonChromeRefreshIcon)
          .Build());
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
    root_action_item_->AddChild(
        ChromeMenuAction(base::BindRepeating(
                             [](Browser* browser, actions::ActionItem* item,
                                actions::ActionInvocationContext context) {
                               chrome::OpenFeedbackDialog(
                                   browser,
                                   feedback::kFeedbackSourceDesktopTabGroups,
                                   /*description_template=*/std::string(),
                                   /*category_tag=*/"tab_group_share");
                             },
                             base::Unretained(browser)),
                         kActionSendSharedTabGroupFeedback,
                         IDS_DATA_SHARING_SHARED_GROUPS_FEEDBACK,
                         IDS_DATA_SHARING_SHARED_GROUPS_FEEDBACK,
                         vector_icons::kFeedbackIcon)
            .Build());
  }

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* toolbar_button_provider =
                    BrowserView::GetBrowserViewForBrowser(browser)
                        ->toolbar_button_provider();
                if (toolbar_button_provider) {
                  toolbar_button_provider->GetPinnedToolbarActionsContainer()
                      ->UpdatePinnedStateAndAnnounce(
                          context.GetProperty(kActionIdKey), true);
                }
              },
              base::Unretained(browser)))
          .SetActionId(kActionPinActionToToolbar)
          .SetImage(ui::ImageModel::FromVectorIcon(kKeepIcon, ui::kColorIcon))
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(
                  IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN)))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* toolbar_button_provider =
                    BrowserView::GetBrowserViewForBrowser(browser)
                        ->toolbar_button_provider();
                if (toolbar_button_provider) {
                  toolbar_button_provider->GetPinnedToolbarActionsContainer()
                      ->UpdatePinnedStateAndAnnounce(
                          context.GetProperty(kActionIdKey), false);
                }
              },
              base::Unretained(browser)))
          .SetActionId(kActionUnpinActionFromToolbar)
          .SetImage(
              ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon))
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(
                  IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN)))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ExecuteCommand(browser,
                                       IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR);
              },
              base::Unretained(browser)))
          .SetActionId(kActionSidePanelShowCustomizeChromeToolbar)
          .SetImage(
              ui::ImageModel::FromVectorIcon(kSettingsMenuIcon, ui::kColorIcon))
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SHOW_CUSTOMIZE_CHROME_TOOLBAR)))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                web_app::ShowPwaInstallDialog(browser);
              },
              base::Unretained(browser)))
          .SetActionId(kActionInstallPwa)
          .SetImage(ui::ImageModel::FromVectorIcon(
              kInstallDesktopChromeRefreshIcon, ui::kColorIcon))
          .SetProperty(actions::kActionItemPinnableKey, false)
          // Text and TooltipText are not populated yet because they are
          // dynamic. They depend on the current tab WebContents.
          .Build());

  // Actions that do not directly show up in chrome UI.
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                browser->GetBrowserView().Cut();
              },
              base::Unretained(browser)))
          .SetActionId(actions::kActionCut)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                browser->GetBrowserView().Copy();
              },
              base::Unretained(browser)))
          .SetActionId(actions::kActionCopy)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                browser->GetBrowserView().Paste();
              },
              base::Unretained(browser)))
          .SetActionId(actions::kActionPaste)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Browser* browser, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                browser->command_controller()->ShowCustomizeChromeSidePanel(
                    CustomizeChromeSection::kFooter);
              },
              base::Unretained(browser)))
          .SetActionId(kActionSidePanelShowCustomizeChromeFooter)
          .Build());

  if (CommentsSidePanelCoordinator::IsSupported()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kComments,
                        IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE,
                        IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE,
                        vector_icons::kChatIcon, kActionSidePanelShowComments,
                        bwi, false)
            .Build());
  }

  AddListeners();
}

void BrowserActions::AddListeners() {
  browser_action_prefs_listener_ = std::make_unique<BrowserActionPrefsListener>(
      base::to_address(profile_), this);
}
