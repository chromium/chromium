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
#include "base/functional/callback_helpers.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/autofill/address_bubbles_icon_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
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
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_util.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"
#include "chrome/browser/ui/views/commerce/discounts_page_action_view_controller.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/js_optimization/js_optimizations_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_controller.h"
#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
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
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/commerce/core/metrics/discounts_metric_collector.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
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
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
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
  TabStripModel* const tab_strip_model = bwi_->GetTabStripModel();
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

  if (features::IsReadAnythingOmniboxChipEnabled() ||
      features::IsImmersiveReadAnythingEnabled()) {
    actions::ActionItem::InvokeActionCallback read_anything_callback =
        base::BindRepeating(
            [](BrowserWindowInterface* bwi, actions::ActionItem* item,
               actions::ActionInvocationContext context) {
              if (!bwi) {
                return;
              }
              read_anything::ReadAnythingEntryPointController::InvokePageAction(
                  bwi, context);
            },
            bwi);
    root_action_item_->AddChild(
        actions::ActionItem::Builder(read_anything_callback)
            .SetActionId(kActionSidePanelShowReadAnything)
            .SetText(l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE))
            .SetTooltipText(l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE))
            .SetImage(ui::ImageModel::FromVectorIcon(kMenuBookChromeRefreshIcon,
                                                     ui::kColorIcon))
            .SetProperty(
                actions::kActionItemPinnableKey,
                static_cast<
                    std::underlying_type_t<actions::ActionPinnableState>>(
                    actions::ActionPinnableState::kPinnable))
            .Build());
  } else {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kReadAnything, IDS_READING_MODE_TITLE,
                        IDS_READING_MODE_TITLE, kMenuBookChromeRefreshIcon,
                        kActionSidePanelShowReadAnything, bwi, true)
            .Build());
  }

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

  if (base::FeatureList::IsEnabled(
          content_settings::features::
              kBlockV8OptimizerOnUnfamiliarSitesSetting)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  views::View* anchor_view =
                      bwi->GetBrowserForMigrationOnly()
                          ->GetBrowserView()
                          .toolbar_button_provider()
                          ->GetAnchorView(kActionShowJsOptimizationsIcon);

                  bwi->GetActiveTabInterface()
                      ->GetTabFeatures()
                      ->js_optimizations_page_action_controller()
                      ->ShowBubble(anchor_view, item);
                },
                bwi))
            .SetActionId(kActionShowJsOptimizationsIcon)
            .SetTooltipText(l10n_util::GetStringUTF16(
                IDS_JS_OPTIMIZATIONS_DISABLED_ICON_TOOLTIP))
            .SetImage(ui::ImageModel::FromVectorIcon(
                // TODO(crbug.com/457422266): Use v8 icon.
                vector_icons::kCodeIcon, ui::kColorIcon,
                ui::SimpleMenuModel::kDefaultIconSize))
            .SetEnabled(true)
            .Build());
  }

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

  // The action does nothing, but is used to configure the page action, which
  // acts as an anchor for the find bar.
  root_action_item_->AddChild(
      actions::ActionItem::Builder(base::DoNothing())
          .SetActionId(kActionFind)
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FIND))
          .SetImage(ui::ImageModel::FromVectorIcon(
              omnibox::kFindInPageChromeRefreshIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* tab_interface =
                    bwi->GetActiveTabInterface();
                CHECK(tab_interface);

                content::WebContents* web_contents =
                    tab_interface->GetContents();
                CHECK(web_contents);

                autofill::VirtualCardEnrollBubbleControllerImpl* controller =
                    autofill::VirtualCardEnrollBubbleControllerImpl::
                        FromWebContents(web_contents);
                CHECK(controller);

                controller->ReshowBubble();
              },
              bwi))
          .SetActionId(kActionVirtualCardEnroll)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_FALLBACK_ICON_TOOLTIP))
          .SetImage(
              ui::ImageModel::FromVectorIcon(kCreditCardChromeRefreshIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* tab_interface =
                    bwi->GetActiveTabInterface();
                CHECK(tab_interface);

                content::WebContents* web_contents =
                    tab_interface->GetContents();
                CHECK(web_contents);

                autofill::FilledCardInformationBubbleControllerImpl*
                    controller =
                        autofill::FilledCardInformationBubbleControllerImpl::
                            FromWebContents(web_contents);
                CHECK(controller);

                controller->ReshowBubble();
              },
              bwi))
          .SetActionId(kActionFilledCardInformation)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_FILLED_CARD_INFORMATION_ICON_TOOLTIP_VIRTUAL_CARD))
          .SetImage(
              ui::ImageModel::FromVectorIcon(kCreditCardChromeRefreshIcon))
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
                auto* page_action_controller =
                    commerce::DiscountsPageActionViewController::From(
                        *bwi->GetActiveTabInterface());
                CHECK(page_action_controller);
                page_action_controller->MaybeShowBubble(/*from_user=*/true);

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
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* tab_interface = bwi->GetActiveTabInterface();
                CHECK(tab_interface);

                autofill::MandatoryReauthBubbleControllerImpl::FromWebContents(
                    tab_interface->GetContents())
                    ->QueueOrShowBubble(/*force_show=*/true);
              },
              bwi))
          .SetActionId(kActionAutofillMandatoryReauth)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_MANDATORY_REAUTH_ICON_TOOLTIP))
          .SetImage(
              ui::ImageModel::FromVectorIcon(kCreditCardChromeRefreshIcon))
          .Build());

  //------- Chrome Menu Actions --------//
  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](Profile* profile, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                CHECK(IncognitoModePrefs::IsIncognitoAllowed(profile));
                chrome::NewIncognitoWindow(profile);
              },
              profile),
          kActionNewIncognitoWindow, IDS_NEW_INCOGNITO_WINDOW,
          IDS_NEW_INCOGNITO_WINDOW, kIncognitoRefreshMenuIcon)
          .SetEnabled(IncognitoModePrefs::IsIncognitoAllowed(profile))
          .Build());

  // Both TabSearch in the toolbar and in Vertical Tabs implementations use
  // ActionItems to represent the 'TabSearch' action.
  if (features::HasTabSearchToolbarButton() ||
      tabs::IsVerticalTabsFeatureEnabled()) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::ShowTabSearch(bwi);
                },
                bwi),
            kActionTabSearch, IDS_TAB_SEARCH_MENU, IDS_TAB_SEARCH_MENU,
            vector_icons::kTabSearchIcon)
            .Build());
  }

  if (tabs::IsVerticalTabsFeatureEnabled()) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* controller =
                      bwi->GetFeatures().vertical_tab_strip_state_controller();
                  controller->SetCollapsed(!controller->IsCollapsed());
                },
                bwi))
            .SetActionId(kActionToggleCollapseVertical)
            .SetText(BrowserActions::GetCleanTitleAndTooltipText(
                l10n_util::GetStringUTF16(IDS_COLLAPSE_VERTICAL_TABS)))
            .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
                l10n_util::GetStringUTF16(IDS_COLLAPSE_VERTICAL_TABS)))
            .Build());
  }

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::NewTab(bwi->GetBrowserForMigrationOnly());
              },
              bwi))
          .SetActionId(kActionNewTab)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_TAB)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_TAB)))
          .SetImage(ui::ImageModel::FromVectorIcon(kAddIcon, ui::kColorIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                // This functionality is controlled by the MenuButtonController.
                // It should have a callback for ShowEverythingMenu.
              },
              bwi))
          .SetActionId(kActionTabGroupsMenu)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SAVED_TAB_GROUPS_MENU)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SAVED_TAB_GROUPS_MENU)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              kSavedTabGroupBarEverythingIcon, ui::kColorIcon))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::Print(bwi);
              },
              bwi),
          kActionPrint, IDS_PRINT, IDS_PRINT, kPrintMenuIcon)
          .SetEnabled(chrome::CanPrint(bwi))
          .Build());

  const bool is_incognito = profile_->IsIncognitoProfile();
  root_action_item_->AddChild(
      ChromeMenuAction(base::BindRepeating(
                           [](BrowserWindowInterface* bwi, bool is_incognito,
                              actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             Browser* const browser_for_opening_webui =
                                 bwi->GetBrowserForMigrationOnly()
                                     ->GetBrowserForOpeningWebUi();
                             if (is_incognito) {
                               chrome::ShowIncognitoClearBrowsingDataDialog(
                                   browser_for_opening_webui);
                             } else {
                               chrome::ShowClearBrowsingDataDialog(
                                   browser_for_opening_webui);
                             }
                           },
                           bwi, is_incognito),
                       kActionClearBrowsingData, IDS_CLEAR_BROWSING_DATA,
                       IDS_CLEAR_BROWSING_DATA, kTrashCanRefreshIcon)
          .SetEnabled(is_incognito ||
                      (!is_guest_session && !profile->IsSystemProfile()))
          .Build());

  if (chrome::CanOpenTaskManager()) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::OpenTaskManager(bwi);
                },
                bwi),
            kActionTaskManager, IDS_TASK_MANAGER, IDS_TASK_MANAGER,
            kTaskManagerIcon)
            .Build());
  }

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleDevToolsWindow(
                    bwi, DevToolsToggleAction::Show(),
                    DevToolsOpenedByAction::kPinnedToolbarButton);
              },
              bwi),
          kActionDevTools, IDS_DEV_TOOLS, IDS_DEV_TOOLS, kDeveloperToolsIcon)
          .Build());

  if (send_tab_to_self::SendTabToSelfToolbarIconController::CanShowOnBrowser(
          bwi)) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, TabStripModel* tab_strip_model,
                   actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* const bubble_controller =
                      bwi->GetFeatures()
                          .send_tab_to_self_toolbar_bubble_controller();
                  if (bubble_controller->IsBubbleShowing()) {
                    bubble_controller->HideBubble();
                  } else {
                    send_tab_to_self::ShowBubble(
                        tab_strip_model->GetActiveWebContents());
                  }
                },
                bwi, tab_strip_model),
            kActionSendTabToSelf, IDS_SEND_TAB_TO_SELF, IDS_SEND_TAB_TO_SELF,
            kDevicesChromeRefreshIcon)
            .SetEnabled(chrome::CanSendTabToSelf(bwi))
            .SetVisible(!sharing_hub::SharingIsDisabledByPolicy(profile))
            .Build());
  }

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowTranslateBubble(bwi);
              },
              bwi),
          kActionShowTranslate, IDS_SHOW_TRANSLATE, IDS_TOOLTIP_TRANSLATE,
          kTranslateIcon)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface& tab =
                    CHECK_DEREF(bwi->GetActiveTabInterface());
                auto* controller =
                    CookieControlsPageActionController::From(tab);
                CHECK(controller);
                controller->ExecutePageAction(bwi->GetBrowserForMigrationOnly()
                                                  ->GetBrowserView()
                                                  .toolbar_button_provider());
              },
              bwi))
          .SetActionId(kActionShowCookieControls)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                std::underlying_type_t<page_actions::PageActionTrigger>
                    page_action_trigger = context.GetProperty(
                        page_actions::kPageActionTriggerKey);
                if (page_action_trigger !=
                    page_actions::kInvalidPageActionTrigger) {
                  BookmarkPageActionController::RecordPageActionExecution(
                      static_cast<page_actions::PageActionTrigger>(
                          page_action_trigger));
                }

                chrome::ExecuteCommand(bwi, IDC_BOOKMARK_THIS_TAB);
              },
              bwi))
          .SetActionId(kActionBookmarkThisTab)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::GenerateQRCode(bwi);
              },
              bwi),
          kActionQrCodeGenerator, IDS_APP_MENU_CREATE_QR_CODE,
          IDS_APP_MENU_CREATE_QR_CODE, kQrCodeChromeRefreshIcon)
          .SetEnabled(false)
          .SetVisible(!sharing_hub::SharingIsDisabledByPolicy(profile))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, TabStripModel* tab_strip_model,
                 actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto page_action_trigger =
                    context.GetProperty(page_actions::kPageActionTriggerKey);
                // If triggered by omnibox page action, do nothing.
                if (page_action_trigger !=
                    page_actions::kInvalidPageActionTrigger) {
                  return;
                }

                auto* controller = autofill::AddressBubblesIconController::Get(
                    tab_strip_model->GetActiveWebContents());
                if (controller && controller->GetBubbleView()) {
                  controller->GetBubbleView()->Hide();
                } else {
                  chrome::ShowAddresses(bwi);
                }
              },
              bwi, tab_strip_model),
          kActionShowAddressesBubbleOrPage,
          IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
          IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
          vector_icons::kLocationOnChromeRefreshIcon)
          .SetEnabled(!is_guest_session)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, TabStripModel* tab_strip_model,
                 actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto page_action_trigger =
                    context.GetProperty(page_actions::kPageActionTriggerKey);
                // When page action is migrated, clicking on the omnibox page
                // should not close the bubble or navigate to `Payment Methods`
                // settings page.
                // Page action trigger is a valid value only when this action
                // is triggered from the migrated page action icon.
                if (page_action_trigger !=
                    page_actions::kInvalidPageActionTrigger) {
                  return;
                }

                auto hide_bubble = [tab_strip_model](int command_id) -> bool {
                  auto* controller = autofill::SavePaymentIconController::Get(
                      tab_strip_model->GetActiveWebContents(), command_id);
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
                  chrome::ShowPaymentMethods(bwi);
                }
              },
              bwi, tab_strip_model),
          kActionShowPaymentsBubbleOrPage, IDS_PAYMENT_METHOD_SUBMENU_OPTION,
          IDS_PAYMENT_METHOD_SUBMENU_OPTION, kCreditCardChromeRefreshIcon)
          .SetEnabled(!is_guest_session)
          .Build());

  // TODO(crbug.com/435220196): Ideally this action would have
  // ChromeLabsCoordinator passed in as a dependency directly.
  if (IsChromeLabsEnabled() && !web_app::AppBrowserController::IsWebApp(bwi)) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  bwi->GetFeatures().chrome_labs_coordinator()->ShowOrHide();
                },
                bwi),
            kActionShowChromeLabs, IDS_CHROMELABS, IDS_CHROMELABS, kScienceIcon)
            .SetVisible(ShouldShowChromeLabsUI(profile))
            .Build());
  }

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, TabStripModel* tab_strip_model,
                 actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                if (PasswordsModelDelegateFromWebContents(web_contents)
                        ->GetState() == password_manager::ui::INACTIVE_STATE) {
                  chrome::ShowPasswordManager(bwi);
                } else {
                  auto* const controller =
                      ManagePasswordsUIController::FromWebContents(
                          web_contents);
                  if (controller->IsShowingBubble()) {
                    controller->HideBubble(
                        /*initiated_by_bubble_manager=*/false);
                  } else {
                    chrome::ManagePasswordsForPage(bwi);
                  }
                }
              },
              bwi, tab_strip_model),
          kActionShowPasswordsBubbleOrPage, IDS_VIEW_PASSWORDS,
          IDS_VIEW_PASSWORDS, vector_icons::kPasswordManagerIcon)
          .SetEnabled(!is_guest_session)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](TabStripModel* tab_strip_model, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                const GURL& url = chrome::GetURLToBookmark(web_contents);
                IntentPickerTabHelper* const intent_picker_tab_helper =
                    IntentPickerTabHelper::FromWebContents(web_contents);
                CHECK(intent_picker_tab_helper);
                intent_picker_tab_helper->ShowIntentPickerBubbleOrLaunchApp(
                    url);
              },
              tab_strip_model))
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
              [](TabStripModel* tab_strip_model, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                // Show the File System Access bubble if applicable for
                // the current page state.
                FileSystemAccessBubbleController::Show(
                    tab_strip_model->GetActiveWebContents());
              },
              tab_strip_model))
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
              [](BrowserWindowInterface* bwi, TabStripModel* tab_strip_model,
                 actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CopyURL(bwi, tab_strip_model->GetActiveWebContents());
              },
              bwi, tab_strip_model),
          kActionCopyUrl, IDS_APP_MENU_COPY_LINK, IDS_APP_MENU_COPY_LINK,
          kLinkChromeRefreshIcon)
          .SetEnabled(chrome::CanCopyUrl(bwi))
          .SetVisible(!sharing_hub::SharingIsDisabledByPolicy(profile))
          .Build());

  // TODO(crbug.com/435220196): Ideally this action would have
  // CastBrowserController passed in as a dependency directly.
  actions::ActionItem* media_router_action;
  root_action_item_->AddChild(
      StatefulChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                // TODO(crbug.com/356468503): Figure out how to capture
                // action invocation location.
                auto* cast_browser_controller =
                    bwi->GetFeatures().cast_browser_controller();
                if (cast_browser_controller) {
                  cast_browser_controller->ToggleDialog();
                }
              },
              bwi),
          kActionRouteMedia, IDS_MEDIA_ROUTER_MENU_ITEM_TITLE,
          IDS_MEDIA_ROUTER_ICON_TOOLTIP_TEXT, kCastChromeRefreshIcon)
          .SetEnabled(chrome::CanRouteMedia(bwi))
          .CopyAddressTo(&media_router_action)
          .Build());
  CastToolbarButtonUtil::AddCastChildActions(media_router_action, bwi);

#if !BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/435220196): Ideally this action would have
  // DownloadToolbarUIController passed in as a dependency directly.
  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetFeatures().download_toolbar_ui_controller()->InvokeUI();
              },
              bwi),
          kActionShowDownloads, IDS_SHOW_DOWNLOADS, IDS_TOOLTIP_DOWNLOAD_ICON,
          kDownloadToolbarButtonChromeRefreshIcon)
          .Build());
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups()) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::OpenFeedbackDialog(
                      bwi, feedback::kFeedbackSourceDesktopTabGroups,
                      /*description_template=*/std::string(),
                      /*category_tag=*/"tab_group_share");
                },
                bwi),
            kActionSendSharedTabGroupFeedback,
            IDS_DATA_SHARING_SHARED_GROUPS_FEEDBACK,
            IDS_DATA_SHARING_SHARED_GROUPS_FEEDBACK,
            vector_icons::kFeedbackIcon)
            .Build());
  }

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ToolbarButtonProvider* toolbar_button_provider =
                    bwi->GetBrowserForMigrationOnly()
                        ->GetBrowserView()
                        .toolbar_button_provider();
                CHECK(toolbar_button_provider);

                views::View* page_action_view =
                    toolbar_button_provider->GetPageActionView(
                        kActionShowCollaborationRecentActivity);
                CHECK(page_action_view);

                tabs::TabInterface* tab = bwi->GetActiveTabInterface();
                CHECK(tab);

                Profile* profile = bwi->GetProfile();
                CHECK(profile);

                RecentActivityBubbleCoordinator* bubble_coordinator =
                    RecentActivityBubbleCoordinator::From(bwi);
                CHECK(bubble_coordinator);

                const std::optional<tab_groups::TabGroupId> group =
                    tab->GetGroup();
                CHECK(group.has_value());

                const tab_groups::TabGroupId group_id = group.value();
                int32_t tab_id = tab->GetHandle().raw_value();
                auto* web_contents = tab->GetContents();

                const std::vector<collaboration::messaging::ActivityLogItem>
                    tab_activity_log =
                        tab_groups::SavedTabGroupUtils::GetRecentActivity(
                            profile, group_id, tab_id);
                const std::vector<collaboration::messaging::ActivityLogItem>
                    group_activity_log =
                        tab_groups::SavedTabGroupUtils::GetRecentActivity(
                            profile, group_id);

                bubble_coordinator->ShowForCurrentTab(
                    page_action_view, web_contents, tab_activity_log,
                    group_activity_log, profile);
              },
              bwi))
          .SetActionId(kActionShowCollaborationRecentActivity)
          .SetImage(ui::ImageModel().FromVectorIcon(
              kPersonFilledPaddedSmallIcon, ui::kColorIcon))
          .Build());

  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(bwi->GetProfile());
  if (OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(
          aim_eligibility_service)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  bool via_keyboard = false;

                  std::underlying_type_t<page_actions::PageActionTrigger>
                      page_action_trigger = context.GetProperty(
                          page_actions::kPageActionTriggerKey);

                  if ((page_action_trigger !=
                       page_actions::kInvalidPageActionTrigger) &&
                      page_action_trigger ==
                          base::to_underlying(
                              page_actions::PageActionTrigger::kKeyboard)) {
                    via_keyboard = true;
                  }

                  tabs::TabInterface* active_tab = bwi->GetActiveTabInterface();
                  CHECK(active_tab);

                  content::WebContents* web_contents =
                      active_tab->GetContents();
                  CHECK(web_contents);

                  OmniboxController* omnibox_controller =
                      search::GetOmniboxController(web_contents);
                  CHECK(omnibox_controller);

                  omnibox::AiModePageActionController::OpenAiMode(
                      *omnibox_controller, via_keyboard);
                },
                bwi))
            .SetActionId(kActionAiMode)
            .SetText(l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL))
            .SetTooltipText(l10n_util::GetStringUTF16(
                IDS_STARTER_PACK_AI_MODE_ACTION_SUGGESTION_CONTENTS))
            .SetImage(ui::ImageModel::FromVectorIcon(omnibox::kSearchSparkIcon))
            .SetProperty(actions::kActionItemPinnableKey, false)
            .Build());
  }

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* active_tab = bwi->GetActiveTabInterface();
                CHECK(active_tab);

                std::underlying_type_t<page_actions::PageActionTrigger>
                    page_action_trigger = context.GetProperty(
                        page_actions::kPageActionTriggerKey);
                CHECK_NE(page_action_trigger,
                         page_actions::kInvalidPageActionTrigger);

                LensOverlayHomeworkPageActionController::From(*active_tab)
                    ->HandlePageActionEvent(
                        static_cast<page_actions::PageActionTrigger>(
                            page_action_trigger) ==
                        page_actions::PageActionTrigger::kKeyboard);
              },
              bwi))
          .SetActionId(kActionLensOverlayHomework)
          .SetImage(ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
              vector_icons::kGoogleLensMonochromeLogoIcon
#else
              vector_icons::kSearchChromeRefreshIcon
#endif
              ))
          .SetText(l10n_util::GetStringUTF16(
              IDS_CONTENT_LENS_OVERLAY_ASK_GOOGLE_ENTRYPOINT_LABEL))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* toolbar_button_provider =
                    bwi->GetBrowserForMigrationOnly()
                        ->GetBrowserView()
                        .toolbar_button_provider();
                if (toolbar_button_provider) {
                  toolbar_button_provider->GetPinnedToolbarActionsContainer()
                      ->UpdatePinnedStateAndAnnounce(
                          context.GetProperty(kActionIdKey), true);
                }
              },
              bwi))
          .SetActionId(kActionPinActionToToolbar)
          .SetImage(ui::ImageModel::FromVectorIcon(kKeepIcon, ui::kColorIcon))
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(
                  IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_PIN)))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* toolbar_button_provider =
                    bwi->GetBrowserForMigrationOnly()
                        ->GetBrowserView()
                        .toolbar_button_provider();
                if (toolbar_button_provider) {
                  toolbar_button_provider->GetPinnedToolbarActionsContainer()
                      ->UpdatePinnedStateAndAnnounce(
                          context.GetProperty(kActionIdKey), false);
                }
              },
              bwi))
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
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ExecuteCommand(bwi, IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR);
              },
              bwi))
          .SetActionId(kActionSidePanelShowCustomizeChromeToolbar)
          .SetImage(
              ui::ImageModel::FromVectorIcon(kSettingsMenuIcon, ui::kColorIcon))
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SHOW_CUSTOMIZE_CHROME_TOOLBAR)))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                web_app::ShowPwaInstallDialog(bwi);
              },
              bwi))
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
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetBrowserForMigrationOnly()->GetBrowserView().Cut();
              },
              bwi))
          .SetActionId(actions::kActionCut)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetBrowserForMigrationOnly()->GetBrowserView().Copy();
              },
              bwi))
          .SetActionId(actions::kActionCopy)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetBrowserForMigrationOnly()->GetBrowserView().Paste();
              },
              bwi))
          .SetActionId(actions::kActionPaste)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](chrome::BrowserCommandController* browser_command_controller,
                 actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                browser_command_controller->ShowCustomizeChromeSidePanel(
                    SidePanelOpenTrigger::kNewTabFooter,
                    CustomizeChromeSection::kFooter);
              },
              bwi->GetFeatures().browser_command_controller()))
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

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    actions::ActionItem::InvokeActionCallback contextual_task_callback =
        base::BindRepeating(
            [](BrowserWindowInterface* bwi, actions::ActionItem* item,
               actions::ActionInvocationContext context) {
              if (!bwi) {
                return;
              }
              chrome::ToggleContextualTasksSidePanel(bwi);
            },
            bwi);
    root_action_item_->AddChild(
        actions::ActionItem::Builder(contextual_task_callback)
            .SetActionId(kActionSidePanelShowContextualTasks)
            .SetText(l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TITLE))
            .SetTooltipText(l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TITLE))
            .SetImage(ui::ImageModel::FromVectorIcon(kDockToRightSparkIcon,
                                                     ui::kColorIcon))
            .SetProperty(
                actions::kActionItemPinnableKey,
                static_cast<
                    std::underlying_type_t<actions::ActionPinnableState>>(
                    actions::ActionPinnableState::kNotPinnable))
            .Build());
  }
// TODO(crbug.com/454112198): Delete this after Multi Instance launches. This
// is currently only used in the experimental single instance side panel.
#if BUILDFLAG(ENABLE_GLIC)
  auto* glic_service = glic::GlicKeyedService::Get(bwi->GetProfile());
  if (glic_service && !glic::GlicEnabling::IsMultiInstanceEnabled()) {
    actions::ActionItem::InvokeActionCallback toggle_glic_callback =
        base::BindRepeating(
            [](base::WeakPtr<BrowserWindowInterface> bwi,
               actions::ActionItem* item,
               actions::ActionInvocationContext context) {
              if (!bwi) {
                return;
              }
              if (auto* glic_service =
                      glic::GlicKeyedService::Get(bwi->GetProfile())) {
                // TODO: create a new invocation source if we end up
                // keeping toolbar icon
                glic_service->ToggleUI(
                    bwi.get(), /*prevent_close=*/false,
                    glic::mojom::InvocationSource::kTopChromeButton);
              }
            },
            bwi->GetWeakPtr());

    root_action_item_->AddChild(
        actions::ActionItem::Builder(toggle_glic_callback)
            .SetActionId(kActionSidePanelShowGlic)
            .SetText(l10n_util::GetStringUTF16(IDS_SETTINGS_GLIC_PAGE_TITLE))
            .SetTooltipText(
                l10n_util::GetStringUTF16(IDS_SETTINGS_GLIC_PAGE_TITLE))
            .SetImage(ui::ImageModel::FromVectorIcon(
                glic::GlicVectorIconManager::GetVectorIcon(
                    IDR_GLIC_BUTTON_VECTOR_ICON),
                ui::kColorIcon))
            .SetProperty(actions::kActionItemPinnableKey, true)
            .Build());
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  AddListeners();
}

void BrowserActions::AddListeners() {
  browser_action_prefs_listener_ = std::make_unique<BrowserActionPrefsListener>(
      base::to_address(profile_), this);
}
