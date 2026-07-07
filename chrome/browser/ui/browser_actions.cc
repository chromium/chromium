// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_actions.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/types/to_address.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#if BUILDFLAG(IS_MAC)
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#endif
#include "base/strings/escape.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_policy_dialog.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_action_properties.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/autofill/address_bubbles_icon_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"
#include "chrome/browser/ui/autofill/payments/save_payment_icon_controller.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_action_prefs_listener.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_select_file_dialog_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_string_utils.h"
#include "chrome/browser/ui/omnibox/ai_mode_page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_controller.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_util.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"
#include "chrome/browser/ui/views/commerce/discounts_page_action_view_controller.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/js_optimization/js_optimizations_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/lens_overlay_homework_page_action_controller.h"
#include "chrome/browser/ui/views/location_bar/record_replay_page_action_controller.h"
#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/side_panel/comments/comments_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_utils.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_coordinator.h"
#include "chrome/browser/ui/views/tabs/groups/recent_activity_bubble_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/ai_overlay_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/ui/webui/inspect/inspect_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/commerce/core/metrics/discounts_metric_collector.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/pref_names.h"
#include "components/multistep_filter/core/features.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/undo/bookmark_undo_service.h"
#include "content/public/common/profiling.h"
#include "extensions/common/extension_urls.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/browser_commands_chromeos.h"
#endif
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "printing/buildflags/buildflags.h"
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

#if BUILDFLAG(IS_LINUX)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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
  if (root_action_item_) {
    // Extract the unique ptr and destruct it after the raw_ptr to avoid a
    // dangling pointer scenario.
    std::unique_ptr<actions::ActionItem> owned_root_action_item =
        actions::ActionManager::Get().RemoveAction(root_action_item_);
    root_action_item_ = nullptr;
  }
}

// static
std::u16string BrowserActions::GetCleanTitleAndTooltipText(
    std::u16string string) {
  return chrome::GetCleanTitleAndTooltipText(std::move(string));
}

void BrowserActions::InitializeBrowserActions() {
  actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().CopyAddressTo(&root_action_item_).Build());

  InitializeSidePanelActions();

  InitializePageActionIconActions();

  InitializeChromeMenuActions();

  InitializeToolbarAndMiscActions();

  InitializeNavigationActions();

  AddListeners();
}

actions::ActionItem* BrowserActions::RegisterAction(
    std::unique_ptr<actions::ActionItem> action_item) {
  return root_action_item_->AddChild(std::move(action_item));
}

void BrowserActions::InitializeSidePanelActions() {
  Profile* const profile = base::to_address(profile_);
  BrowserWindowInterface* const bwi = base::to_address(bwi_);

  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kBookmarks, IDS_BOOKMARK_MANAGER_TITLE,
                      IDS_BOOKMARK_MANAGER_TITLE,
                      features::IsRoundedIconsEnabled()
                          ? kHotelClassIcon
                          : kBookmarksSidePanelRefreshOldIcon,
                      kActionSidePanelShowBookmarks, bwi, true)
          .Build());
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kReadingList, IDS_READ_LATER_TITLE,
                      IDS_READ_LATER_TITLE,
                      features::IsRoundedIconsEnabled() ? kListAltIcon
                                                        : kReadingListOldIcon,
                      kActionSidePanelShowReadingList, bwi, true)
          .Build());
  if (TabsFromOtherDevicesSidePanelCoordinator::IsSupported(profile)) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kTabsFromOtherDevices,
                        IDS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TITLE,
                        IDS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TITLE,
                        features::IsRoundedIconsEnabled()
                            ? kDevicesIcon
                            : kDevicesChromeRefreshOldIcon,
                        kActionSidePanelShowTabsFromOtherDevices, bwi, true)
            .Build());
  }
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kAboutThisSite,
                      IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                      IDS_PAGE_INFO_ABOUT_THIS_PAGE_TITLE,
                      PageInfoViewFactory::GetAboutThisSiteVectorIcon(),
                      kActionSidePanelShowAboutThisSite, bwi, false)
          .Build());
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kCustomizeChrome,
                      IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                      IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE,
                      features::IsRoundedIconsEnabled() ? kEditIcon
                      : features::IsRoundedIconsEnabled()
                          ? vector_icons::kEditIcon
                          : vector_icons::kEditChromeRefreshOldIcon,
                      kActionSidePanelShowCustomizeChrome, bwi, false)
          .Build());
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kShoppingInsights,
                      IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                      IDS_SHOPPING_INSIGHTS_SIDE_PANEL_TITLE,
                      features::IsRoundedIconsEnabled()
                          ? vector_icons::kShoppingBagIcon
                          : vector_icons::kShoppingBagOldIcon,
                      kActionSidePanelShowShoppingInsights, bwi, false)
          .Build());
  root_action_item_->AddChild(
      SidePanelAction(
          SidePanelEntryId::kMerchantTrust, IDS_MERCHANT_TRUST_SIDE_PANEL_TITLE,
          IDS_MERCHANT_TRUST_SIDE_PANEL_TITLE,
          features::IsRoundedIconsEnabled() ? vector_icons::kStorefrontIcon
                                            : vector_icons::kStorefrontOldIcon,
          kActionSidePanelShowMerchantTrust, bwi, false)
          .Build());

  if (side_panel::history_clusters::
          IsHistoryClustersSidePanelSupportedForProfile(profile) &&
      !HistorySidePanelCoordinator::IsSupported()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kHistoryClusters, IDS_HISTORY_TITLE,
                        IDS_HISTORY_CLUSTERS_SHOW_SIDE_PANEL,
                        features::IsRoundedIconsEnabled()
                            ? vector_icons::kHistoryIcon
                            : vector_icons::kHistoryChromeRefreshOldIcon,
                        kActionSidePanelShowHistoryCluster, bwi, true)
            .Build());
  }

  if (HistorySidePanelCoordinator::IsSupported()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kHistory, IDS_HISTORY_TITLE,
                        IDS_HISTORY_SHOW_SIDE_PANEL,
                        features::IsRoundedIconsEnabled()
                            ? vector_icons::kHistoryIcon
                            : vector_icons::kHistoryChromeRefreshOldIcon,
                        kActionSidePanelShowHistory, bwi, true)
            .Build());
  }

  ui::Accelerator reading_mode_accelerator;
  std::u16string reading_mode_shortcut;
  if (GetAcceleratorForCommandId(IDC_SHOW_READING_MODE_KEYBOARD,
                                 &reading_mode_accelerator)) {
    reading_mode_shortcut = reading_mode_accelerator.GetShortcutText();
  }

  if (features::IsReadAnythingOmniboxChipEnabled() ||
      features::IsImmersiveReadAnythingEnabled()) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  read_anything::ReadAnythingEntryPointController::
                      InvokePageAction(bwi, context);
                },
                bwi))
            .SetActionId(kActionSidePanelShowReadAnything)
            .SetText(l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE))
            .SetTooltipText(l10n_util::GetStringFUTF16(IDS_READING_MODE_TOOLTIP,
                                                       reading_mode_shortcut))
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? kMenuBookIcon
                    : kMenuBookChromeRefreshOldIcon,
                ui::kColorIcon))
            .SetProperty(
                actions::kActionItemPinnableKey,
                static_cast<
                    std::underlying_type_t<actions::ActionPinnableState>>(
                    actions::ActionPinnableState::kPinnable))
            .Build());
  } else {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            CreateToggleSidePanelActionCallback(
                SidePanelEntryKey(SidePanelEntryId::kReadAnything), bwi))
            .SetActionId(kActionSidePanelShowReadAnything)
            .SetText(l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE))
            .SetTooltipText(l10n_util::GetStringFUTF16(IDS_READING_MODE_TOOLTIP,
                                                       reading_mode_shortcut))
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? kMenuBookIcon
                    : kMenuBookChromeRefreshOldIcon,
                ui::kColorIcon))
            .SetProperty(
                actions::kActionItemPinnableKey,
                static_cast<
                    std::underlying_type_t<actions::ActionPinnableState>>(
                    actions::ActionPinnableState::kPinnable))
            .Build());
  }

  if (lens::features::IsLensOverlayEnabled()) {
    const gfx::VectorIcon& icon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        vector_icons::kGoogleLensMonochromeLogoIcon;
#else
        features::IsRoundedIconsEnabled()
            ? vector_icons::kSearchIcon
            : vector_icons::kSearchChromeRefreshOldIcon;
#endif
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  if (!bwi) {
                    return;
                  }
                  lens::LensOverlayEntryPointController::InvokeAction(
                      bwi->GetActiveTabInterface(), context);
                },
                bwi))
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

  // Create the lens action item. The icon and text are set appropriately in the
  // lens side panel coordinator. They have default values here.
  root_action_item_->AddChild(
      SidePanelAction(SidePanelEntryId::kLens, IDS_LENS_DEFAULT_TITLE,
                      IDS_LENS_DEFAULT_TITLE,
                      features::IsRoundedIconsEnabled()
                          ? vector_icons::kImageSearchIcon
                          : vector_icons::kImageSearchOldIcon,
                      kActionSidePanelShowLens, bwi, false)
          .Build());

  if (CommentsSidePanelCoordinator::IsSupported()) {
    root_action_item_->AddChild(
        SidePanelAction(SidePanelEntryId::kComments,
                        IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE,
                        IDS_COLLABORATION_SHARED_TAB_GROUPS_COMMENTS_TITLE,
                        features::IsRoundedIconsEnabled()
                            ? vector_icons::kChatIcon
                            : vector_icons::kChatOldIcon,
                        kActionSidePanelShowComments, bwi, false)
            .Build());
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  if (!bwi) {
                    return;
                  }
                  if (contextual_tasks::
                          IsContextualTasksPinButtonInToolbarEnabled() &&
                      contextual_tasks::GetEffectivePinState(
                          bwi->GetProfile())) {
                    chrome::ToggleContextualTasksSidePanelZeroState(bwi);
                  } else {
                    chrome::ToggleContextualTasksSidePanel(bwi);
                  }
                },
                bwi))
            .SetActionId(kActionSidePanelShowContextualTasks)
            .SetText(l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_CUSTOMIZE_CHROME_LABEL))
            .SetTooltipText(l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_CUSTOMIZE_CHROME_LABEL))
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? omnibox::kSearchSparkIcon
                    : omnibox::kSearchSparkOldIcon,
                ui::kColorIcon))
            .SetProperty(
                actions::kActionItemPinnableKey,
                static_cast<
                    std::underlying_type_t<actions::ActionPinnableState>>(
                    actions::ActionPinnableState::kPinnable))
            .SetVisible(
                contextual_tasks::EntryPointEligibilityManager::IsEligible(
                    profile))
            .Build());
  }

  if (glic::GlicEnabling::IsEnabledByGlobalCriteria()) {
    root_action_item_->AddChild(
        SidePanelAction(
            SidePanelEntryId::kGlic, IDS_SETTINGS_SIDE_PANEL_ALIGNMENT_GLIC,
            IDS_SETTINGS_SIDE_PANEL_ALIGNMENT_GLIC, omnibox::kSparkIcon,
            kActionSidePanelShowGlic, bwi, false)
            .SetVisible(glic::GlicEnabling::ShouldShowGlicButton(profile))
            .Build());
  }
}

void BrowserActions::InitializePageActionIconActions() {
  BrowserWindowInterface* const bwi = base::to_address(bwi_);

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
              features::IsRoundedIconsEnabled()
                  ? kShoppingmodeIcon
                  : kLocalOfferFlippedRefreshOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
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
              features::IsRoundedIconsEnabled()
                  ? kSpeedIcon
                  : kPerformanceSpeedometerOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetEnabled(true)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (!bwi) {
                  return;
                }
                if (auto* fedcm_view = AccountSelectionView::Get(
                        bwi->GetActiveTabInterface()
                            ->GetUnownedUserDataHost())) {
                  fedcm_view->OnPageActionClicked();
                }
              },
              bwi))
          .SetActionId(kActionFederation)
          .SetEnabled(true)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* web_contents =
                    bwi->GetActiveTabInterface()->GetContents();
                auto* controller = ambient_signin::AmbientSigninController::
                    GetForCurrentDocument(web_contents->GetPrimaryMainFrame());
                if (!controller) {
                  return;
                }
                controller->TriggerPageActionSignIn();
              },
              bwi))
          .SetActionId(kActionWebAuthnAmbientSignin)
          .SetEnabled(true)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (!bwi) {
                  return;
                }
                auto anchor =
                    bwi->GetBrowserForMigrationOnly()
                        ->GetBrowserView()
                        .toolbar_button_provider()
                        ->GetBubbleAnchor(kActionShowJsOptimizationsIcon);

                bwi->GetActiveTabInterface()
                    ->GetTabFeatures()
                    ->js_optimizations_page_action_controller()
                    ->ShowBubble(anchor, item);
              },
              bwi))
          .SetActionId(kActionShowJsOptimizationsIcon)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_JS_OPTIMIZATIONS_DISABLED_ICON_TOOLTIP))
          .SetImage(ui::ImageModel::FromVectorIcon(
              vector_icons::kShieldIcon, ui::kColorIcon,
              ui::SimpleMenuModel::kDefaultIconSize))
          .SetEnabled(true)
          .Build());

  if (base::FeatureList::IsEnabled(
          record_replay::features::kRecordReplayBase)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  bwi->GetActiveTabInterface()
                      ->GetTabFeatures()
                      ->record_replay_page_action_controller()
                      ->ExecuteAction(item);
                },
                bwi))
            .SetActionId(kActionRecordReplay)
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? vector_icons::kScreenRecordIcon
                    : vector_icons::kScreenRecordOldIcon,
                ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
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
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kZoomInIcon : kZoomInOldIcon))
          .Build());

  // The action does nothing, but is used to configure the page action, which
  // acts as an anchor for the find bar.
  root_action_item_->AddChild(
      actions::ActionItem::Builder(base::DoNothing())
          .SetActionId(kActionFind)
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FIND))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? omnibox::kFindInPageIcon
                  : omnibox::kFindInPageChromeRefreshOldIcon))
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
                if (!controller) {
                  return;
                }
                controller->ReshowBubble();
              },
              bwi))
          .SetActionId(kActionVirtualCardEnroll)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_FALLBACK_ICON_TOOLTIP))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kCreditCardIcon
                  : kCreditCardChromeRefreshOldIcon))
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
                if (!controller) {
                  return;
                }
                controller->ReshowBubble();
              },
              bwi))
          .SetActionId(kActionFilledCardInformation)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_FILLED_CARD_INFORMATION_ICON_TOOLTIP_VIRTUAL_CARD))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kCreditCardIcon
                  : kCreditCardChromeRefreshOldIcon))
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
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kShoppingBagIcon
                  : vector_icons::kShoppingBagRefreshOldIcon))
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
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kShoppingmodeIcon
                  : vector_icons::kShoppingmodeOldIcon))
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
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kCreditCardIcon
                  : kCreditCardChromeRefreshOldIcon))
          .Build());
}

void BrowserActions::InitializeChromeMenuActions() {
  Profile* const profile = base::to_address(profile_);
  TabStripModel* const tab_strip_model = bwi_->GetTabStripModel();
  BrowserWindowInterface* const bwi = base::to_address(bwi_);
  const bool is_guest_session = profile->IsGuestSession();

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
          IDS_NEW_INCOGNITO_WINDOW,
          features::IsRoundedIconsEnabled() ? kIncognitoIcon
                                            : kIncognitoRefreshMenuOldIcon)
          .SetEnabled(IncognitoModePrefs::IsIncognitoAllowed(profile))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowTabSearch(bwi);
              },
              bwi),
          kActionTabSearch, IDS_TAB_SEARCH_MENU, IDS_TAB_SEARCH_MENU,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kKeyboardArrowDownIcon
              : vector_icons::kExpandMoreOldIcon)
          .SetProperty(
              actions::kActionItemPinnableKey,
              static_cast<std::underlying_type_t<actions::ActionPinnableState>>(
                  actions::ActionPinnableState::kNotPinnable))
          .Build());

  if (tabs::IsVerticalTabsFeatureEnabled()) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* controller =
                      tabs::VerticalTabStripStateController::From(bwi);
                  bool collapse =
                      controller->GetCollapseState() ==
                      tabs::VerticalTabStripCollapseState::kExpanded;
                  controller->RequestCollapse(collapse);
                  base::RecordAction(base::UserMetricsAction(
                      collapse
                          ? "VerticalTabs_TabStrip_ButtonToggleCollapsed"
                          : "VerticalTabs_TabStrip_ButtonToggleUncollapsed"));
                },
                bwi))
            .SetActionId(kActionToggleCollapseVertical)
            .SetAccelerator(ui::Accelerator(
                ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR))
            .Build());
  }

  if (tab_groups::IsProjectsPanelFeatureEnabled()) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* controller = ProjectsPanelStateController::From(bwi);
                  if (controller) {
                    controller->SetProjectsVisible(
                        !controller->IsProjectsPanelVisible());
                  }

                  // Dismiss the IPH promo if it is currently showing, or abort
                  // it if it is queued to show.
                  if (auto* interface =
                          BrowserUserEducationInterface::From(bwi)) {
                    const base::Feature& iph_feature =
                        feature_engagement::kIPHResumptionRailFeature;
                    if (interface->IsFeaturePromoActive(iph_feature)) {
                      interface->NotifyFeaturePromoFeatureUsed(
                          iph_feature,
                          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
                    } else if (interface->IsFeaturePromoQueued(iph_feature)) {
                      interface->AbortFeaturePromo(iph_feature);
                    }
                  }
                },
                bwi))
            .SetActionId(kActionToggleProjectsPanel)
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? kGridViewIcon
                    : kSavedTabGroupBarEverythingOldIcon,
                ui::kColorIcon))
            .Build());
  }

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::NewTab(bwi, NewTabTypes::kNewTabCommand);
              },
              bwi))
          .SetActionId(kActionNewTab)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_TAB)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_TAB)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kAddWeight500Icon
                  : vector_icons::kAddOldIcon,
              ui::kColorIcon))
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
              features::IsRoundedIconsEnabled()
                  ? kGridViewIcon
                  : kSavedTabGroupBarEverythingOldIcon,
              ui::kColorIcon))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::Print(bwi);
              },
              bwi),
          kActionPrint, IDS_PRINT, IDS_PRINT,
          features::IsRoundedIconsEnabled() ? kPrintIcon : kPrintMenuOldIcon)
          .SetEnabled(chrome::CanPrint(bwi))
          .Build());

  const bool is_incognito = profile_->IsIncognitoProfile();
  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, bool is_incognito,
                 actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                BrowserWindowInterface* const browser_for_opening_webui =
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
          IDS_CLEAR_BROWSING_DATA,
          features::IsRoundedIconsEnabled() ? kDeleteIcon
                                            : kTrashCanRefreshOldIcon)
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
            features::IsRoundedIconsEnabled() ? kTableChartIcon
                                              : kTaskManagerOldIcon)
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
          kActionDevTools, IDS_DEV_TOOLS, IDS_DEV_TOOLS,
          features::IsRoundedIconsEnabled() ? kCodeIcon
                                            : kDeveloperToolsOldIcon)
          .Build());

  if (send_tab_to_self::SendTabToSelfToolbarIconController::CanShowOnBrowser(
          bwi)) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, TabStripModel* tab_strip_model,
                   actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* const bubble_controller = send_tab_to_self::
                      SendTabToSelfToolbarBubbleController::From(bwi);
                  if (bubble_controller->IsBubbleShowing()) {
                    bubble_controller->HideBubble();
                  } else {
                    send_tab_to_self::ShowBubble(
                        tab_strip_model->GetActiveWebContents(),
                        send_tab_to_self::ShareEntryPoint::kToolbarIcon);
                  }
                },
                bwi, tab_strip_model),
            kActionSendTabToSelf, IDS_SEND_TAB_TO_SELF, IDS_SEND_TAB_TO_SELF,
            features::IsRoundedIconsEnabled() ? kDevicesIcon
                                              : kDevicesChromeRefreshOldIcon)
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
          vector_icons::kGTranslateIcon)
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
          IDS_APP_MENU_CREATE_QR_CODE,
          features::IsRoundedIconsEnabled() ? kQrCodeIcon
                                            : kQrCodeChromeRefreshOldIcon)
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
          features::IsRoundedIconsEnabled()
              ? vector_icons::kLocationOnIcon
              : vector_icons::kLocationOnChromeRefreshOldIcon)
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
          IDS_PAYMENT_METHOD_SUBMENU_OPTION,
          features::IsRoundedIconsEnabled() ? kCreditCardIcon
                                            : kCreditCardChromeRefreshOldIcon)
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
                  ChromeLabsCoordinator::From(bwi)->ShowOrHide();
                },
                bwi),
            kActionShowChromeLabs, IDS_CHROMELABS, IDS_CHROMELABS,
            features::IsRoundedIconsEnabled()   ? kScienceIcon
            : features::IsRoundedIconsEnabled() ? vector_icons::kScienceIcon
                                                : kScienceOldIcon)
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
          IDS_VIEW_PASSWORDS,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kPasswordManagerIcon
              : vector_icons::kPasswordManagerOldIcon)
          .SetEnabled(!is_guest_session)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating([](actions::ActionItem* item,
                                 actions::ActionInvocationContext context) {
            profiles::SwitchToGuestProfile();
          }))
          .SetActionId(kActionOpenGuestProfile)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                InspectUI::InspectDevices(bwi->GetBrowserForMigrationOnly());
              },
              bwi))
          .SetActionId(kActionDevToolsDevices)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleDevToolsWindow(
                    bwi, DevToolsToggleAction::Inspect(),
                    DevToolsOpenedByAction::kInspectorModeShortcut);
              },
              bwi))
          .SetActionId(kActionDevToolsInspect)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating([](actions::ActionItem* item,
                                 actions::ActionInvocationContext context) {
            content::Profiling::Toggle();
          }))
          .SetActionId(kActionProfilingEnabled)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                content::WebContents* const web_contents =
                    bwi->GetTabStripModel()->GetActiveWebContents();
                if (web_contents) {
                  ShowPageInfoDialog(
                      web_contents,
                      base::BindOnce(
                          [](BrowserWindowInterface* bwi,
                             views::Widget::ClosedReason closed_reason,
                             bool reload_prompt) {
                            if (reload_prompt) {
                              return;
                            }
                            if (closed_reason != views::Widget::ClosedReason::
                                                     kEscKeyPressed &&
                                closed_reason != views::Widget::ClosedReason::
                                                     kCloseButtonClicked) {
                              return;
                            }
                            content::WebContents* const active_contents =
                                bwi->GetTabStripModel()->GetActiveWebContents();
                            if (active_contents) {
                              active_contents->Focus();
                            }
                          },
                          bwi),
                      bubble_anchor_util::Anchor::kAppMenuButton);
                }
              },
              bwi))
          .SetActionId(kActionWebAppMenuAppInfo)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetFeatures()
                    .browser_select_file_dialog_controller()
                    ->OpenFile();
              },
              bwi))
          .SetActionId(kActionOpenFile)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowExtensions(bwi);
              },
              bwi))
          .SetActionId(kActionExtensionsSubmenuManageExtensions)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowWebStore(bwi, extension_urls::kAppMenuUtmSource);
              },
              bwi))
          .SetActionId(kActionExtensionsSubmenuVisitChromeWebStore)
          .Build());
}

void BrowserActions::InitializeToolbarAndMiscActions() {
  Profile* const profile = base::to_address(profile_);
  BrowserWindowInterface* const bwi = base::to_address(bwi_);
  TabStripModel* const tab_strip_model = bwi_->GetTabStripModel();

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](TabStripModel* tab_strip_model, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                if (web_contents) {
                  content::RenderFrameHost* const rfh =
                      web_contents->GetFocusedFrame();
                  if (rfh) {
                    rfh->GetRenderWidgetHost()->UpdateTextDirection(
                        base::i18n::LEFT_TO_RIGHT);
                    rfh->GetRenderWidgetHost()->NotifyTextDirection();
                  }
                }
              },
              tab_strip_model))
          .SetActionId(kActionWritingDirectionLtr)
          .SetText(l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](TabStripModel* tab_strip_model, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                content::WebContents* const web_contents =
                    tab_strip_model->GetActiveWebContents();
                if (web_contents) {
                  content::RenderFrameHost* const rfh =
                      web_contents->GetFocusedFrame();
                  if (rfh) {
                    rfh->GetRenderWidgetHost()->UpdateTextDirection(
                        base::i18n::RIGHT_TO_LEFT);
                    rfh->GetRenderWidgetHost()->NotifyTextDirection();
                  }
                }
              },
              tab_strip_model))
          .SetActionId(kActionWritingDirectionRtl)
          .SetText(l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL))
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
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kOpenInNewIcon
                  : kOpenInNewChromeRefreshOldIcon,
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
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kFileSaveIcon
                                                : kFileSaveChromeRefreshOldIcon,
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
          features::IsRoundedIconsEnabled() ? kLinkIcon
                                            : kLinkChromeRefreshOldIcon)
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
          IDS_MEDIA_ROUTER_ICON_TOOLTIP_TEXT,
          features::IsRoundedIconsEnabled() ? kCastIcon
                                            : kCastChromeRefreshOldIcon)
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
                if (auto* controller = DownloadToolbarUIController::From(bwi)) {
                  controller->InvokeUI();
                }
              },
              bwi),
          kActionShowDownloads, IDS_SHOW_DOWNLOADS, IDS_TOOLTIP_DOWNLOAD_ICON,
          features::IsRoundedIconsEnabled()
              ? kDownloadIcon
              : kDownloadToolbarButtonChromeRefreshOldIcon)
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
            features::IsRoundedIconsEnabled() ? vector_icons::kFeedbackIcon
                                              : vector_icons::kFeedbackOldIcon)
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

                views::BubbleAnchor page_action_anchor =
                    toolbar_button_provider->GetPageActionBubbleAnchor(
                        kActionShowCollaborationRecentActivity);
                CHECK(page_action_anchor);

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
                    page_action_anchor, web_contents, tab_activity_log,
                    group_activity_log, profile);
              },
              bwi))
          .SetActionId(kActionShowCollaborationRecentActivity)
          .SetImage(ui::ImageModel().FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kPersonFilledIcon
                  : kPersonFilledPaddedSmallOldIcon,
              ui::kColorIcon))
          .Build());

  auto* ai_mode_button_service =
      AiModeButtonServiceFactory::GetForProfile(base::to_address(profile_));
  if (ai_mode_button_service) {
    // If `ai_mode_button_service` is null, it will remain null and the button
    // will not be needed.
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
                          std::to_underlying(
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
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? omnibox::kSearchSparkIcon
                    : omnibox::kSearchSparkOldIcon))
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
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kSearchIcon
                  : vector_icons::kSearchChromeRefreshOldIcon
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
                  toolbar_button_provider->GetPinnedToolbarActions()
                      ->UpdatePinnedStateAndAnnounce(
                          context.GetProperty(kActionIdKey), true);
                }
              },
              bwi))
          .SetActionId(kActionPinActionToToolbar)
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kKeepIcon : kKeepOldIcon,
              ui::kColorIcon))
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
                  toolbar_button_provider->GetPinnedToolbarActions()
                      ->UpdatePinnedStateAndAnnounce(
                          context.GetProperty(kActionIdKey), false);
                }
              },
              bwi))
          .SetActionId(kActionUnpinActionFromToolbar)
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kKeepOffIcon
                                                : kKeepOffOldIcon,
              ui::kColorIcon))
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
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kSettingsIcon
                                                : kSettingsMenuOldIcon,
              ui::kColorIcon))
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
              features::IsRoundedIconsEnabled()
                  ? kInstallDesktopIcon
                  : kInstallDesktopChromeRefreshOldIcon,
              ui::kColorIcon))
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
                chrome::NewWindow(bwi);
              },
              bwi))
          .SetActionId(kActionNewWindow)
          .Build());
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
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetFeatures()
                    .browser_command_controller()
                    ->ShowCustomizeChromeSidePanel(
                        SidePanelOpenTrigger::kNewTabFooter,
                        CustomizeChromeSection::kFooter);
              },
              bwi))
          .SetActionId(kActionSidePanelShowCustomizeChromeFooter)
          .Build());

  if (base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  if (!bwi || !bwi->GetTabStripModel()) {
                    return;
                  }
                  bwi->GetTabStripModel()->SetFocusedGroup(std::nullopt);
                },
                bwi))
            .SetActionId(kActionUnfocusTabGroup)
            .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
                l10n_util::GetStringUTF16(
                    IDS_TAB_GROUP_HEADER_CXMENU_UNFOCUS_GROUP)))
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled() ? kArrowBackIcon
                : features::IsRoundedIconsEnabled()
                    ? vector_icons::kArrowBackIcon
                    : vector_icons::kArrowBackOldIcon,
                ui::kColorIcon))
            .Build());
  }

  if (glic::GlicEnabling::IsProfileEligible(profile) &&
      base::FeatureList::IsEnabled(features::kAiOverlayDialog)) {
    std::unique_ptr<actions::ActionItem> item =
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  if (auto* controller =
                          ttc::AiOverlayDialogController::From(bwi)) {
                    controller->ToggleOverlay();
                  }
                },
                bwi))
            .SetActionId(kActionShowAiOverlayDialog)
            .SetText(l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP))
            .SetTooltipText(l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP))
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled() ? vector_icons::kMicFilledIcon
                                                  : vector_icons::kMicOldIcon,
                ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
            .SetProperty(
                actions::kActionItemPinnableKey,
                static_cast<
                    std::underlying_type_t<actions::ActionPinnableState>>(
                    actions::ActionPinnableState::kPinnable))
            .Build();

    item->SetProperty(
        kCustomPinnedActionToolbarButtonFactoryKey,
        std::make_unique<CreateCustomPinnedActionToolbarButtonCallback>(
            base::BindRepeating(
                [](Browser* browser, actions::ActionId action_id,
                   base::WeakPtr<PinnedToolbarActionsContainer> container)
                    -> std::unique_ptr<PinnedActionToolbarButton> {
                  return std::make_unique<AiOverlayToolbarButton>(
                      browser, action_id, container);
                })));
    root_action_item_->AddChild(std::move(item));
  }

  // Registration of Gemini in Chrome Anchored Cues, but requires call-time
  // configuration to update the label, button text, and suggested prompt. As
  // such, this action is disabled upon registration, and enabled at call time
  // by OnTriggerAnchoredMessage().
  auto* glic_service = glic::GlicKeyedService::Get(bwi->GetProfile());
  if (glic_service) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating([](actions::ActionItem* item,
                                   actions::ActionInvocationContext context) {
              DUMP_WILL_BE_NOTREACHED()
                  << "Contextual cueing action invoked without being "
                     "configured by OnTriggerAnchoredMessage";
            }))
            .SetActionId(kActionGlicContextualCueing)
            .SetEnabled(false)
            .SetVisible(false)
            .SetText(l10n_util::GetStringUTF16(IDS_SETTINGS_GLIC_PAGE_TITLE))
            .SetImage(ui::ImageModel::FromVectorIcon(
                glic::GlicVectorIconManager::GetVectorIcon(
                    IDR_GLIC_BUTTON_VECTOR_ICON),
                ui::kColorIcon))
            .Build());
  }

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (!bwi) {
                  return;
                }
                auto* tab = bwi->GetActiveTabInterface();
                if (!tab) {
                  return;
                }
                auto* controller =
                    indigo::IndigoPageActionController::From(tab);
                if (controller) {
                  auto entry_point = [&]() {
                    auto raw_entry_point =
                        static_cast<page_actions::PageActionEntryPoint>(
                            context.GetProperty(
                                page_actions::kPageActionEntryPointKey));
                    switch (raw_entry_point) {
                      case page_actions::PageActionEntryPoint::kSuggestionChip:
                        return indigo::EntryPoint::kSuggestionChip;
                      case page_actions::PageActionEntryPoint::kAnchoredMessage:
                        return indigo::EntryPoint::kAnchoredMessage;
                    }
                    NOTREACHED();
                  }();
                  controller->InvokeAction(entry_point);
                }
              },
              bwi))
          .SetActionId(kActionIndigo)
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_INDIGO_ENTRYPOINT_CHIP_TOOLTIP_TEXT))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? vector_icons::kCodeIcon
                                                : vector_icons::kCodeOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetText(l10n_util::GetStringUTF16(IDS_INDIGO_ENTRYPOINT_CHIP_TEXT))
          .Build());

  if (base::FeatureList::IsEnabled(multistep_filter::kMultistepFilter)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder()
            .SetActionId(kActionMultistepFilter)
            .SetInvokeActionCallback(base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  tabs::TabInterface* tab = bwi->GetActiveTabInterface();
                  if (!tab) {
                    return;
                  }
                  multistep_filter::FilterUiController* filter_ui_controller =
                      multistep_filter::FilterUiController::From(tab);
                  if (!filter_ui_controller) {
                    // The controller is null in off-the-record (incognito)
                    // sessions.
                    return;
                  }
                  filter_ui_controller->OnActionInvoked();
                },
                bwi))
            .SetImage(ui::ImageModel::FromVectorIcon(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                vector_icons::kPlayCircleSparkIcon
#else
                features::IsRoundedIconsEnabled()
                    ? vector_icons::kPlayArrowIcon
                    : vector_icons::kPlayArrowChromeRefreshOldIcon
#endif
                ))
            .SetText(
                l10n_util::GetStringUTF16(IDS_MULTISTEP_FILTER_CUE_ACTION_TEXT))
            .Build());
  }

  if (base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder()
            // Anchored message icon, strings and callback are set at cue time.
            .SetActionId(kActionAnchoredContextualCue)
            .Build());
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableOmniboxAutofill)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  if (!bwi) {
                    return;
                  }

                  // Close the IPH if the user clicks the "Autofill payment"
                  // chip displayed on the omnibox.
                  if (auto* user_education =
                          BrowserUserEducationInterface::From(bwi)) {
                    user_education->NotifyFeaturePromoFeatureUsed(
                        feature_engagement::
                            kIPHAutofillOmniboxPaymentChipFeature,
                        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
                  }

                  auto* tab = bwi->GetActiveTabInterface();
                  if (!tab) {
                    return;
                  }

                  // Show the payment method suggestion list after the user
                  // clicks the "Autofill payment" chip displayed on the
                  // omnibox.
                  if (auto* controller =
                          autofill::OmniboxAutofillBubbleController::From(
                              *tab)) {
                    controller->QueueOrShowBubble(/*force_show=*/true);
                  }
                },
                bwi))
            .SetActionId(kActionAutofillPayment)
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? kCreditCardIcon
                    : kCreditCardChromeRefreshOldIcon))
            .SetText(l10n_util::GetStringUTF16(IDS_AUTOFILL_PAYMENT_TEXT))
            .Build());
  }

  // Fake Page Action for Chrome internals page debugging.
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi))
          .SetActionId(kActionFakePageActionForDebug)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (!bwi->GetTabStripModel()->GetActiveTab()->IsSplit()) {
                  chrome::NewSplitTab(
                      bwi, split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kKeyboardShortcut);
                }
              },
              bwi))
          .SetActionId(kActionSplitTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ManagePasswordsForPage(bwi);
              },
              bwi))
          .SetActionId(kActionManagePasswordsForPage)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SaveCreditCard(bwi);
              },
              bwi))
          .SetActionId(kActionSaveCreditCardForPage)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SaveIban(bwi);
              },
              bwi))
          .SetActionId(kActionSaveIbanForPage)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowPasswordManager(bwi);
              },
              bwi),
          kActionShowPasswordManager, IDS_VIEW_PASSWORDS, IDS_VIEW_PASSWORDS,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kPasswordManagerIcon
              : vector_icons::kPasswordManagerOldIcon)
          .SetEnabled(!profile->IsGuestSession())
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowPaymentMethods(bwi);
              },
              bwi),
          kActionShowPaymentMethods, IDS_PAYMENT_METHOD_SUBMENU_OPTION,
          IDS_PAYMENT_METHOD_SUBMENU_OPTION,
          features::IsRoundedIconsEnabled() ? kCreditCardIcon
                                            : kCreditCardChromeRefreshOldIcon)
          .SetEnabled(!profile->IsGuestSession())
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowAddresses(bwi);
              },
              bwi),
          kActionShowAddresses, IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
          IDS_ADDRESSES_AND_MORE_SUBMENU_OPTION,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kLocationOnIcon
              : vector_icons::kLocationOnChromeRefreshOldIcon)
          .SetEnabled(!profile->IsGuestSession())
          .Build());

#if BUILDFLAG(IS_CHROMEOS)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleMultitaskMenu(bwi);
              },
              bwi))
          .SetActionId(kToggleMultitaskMenu)
          .Build());
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](Profile* profile, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                PrefService* prefs = profile->GetPrefs();
                prefs->SetBoolean(
                    prefs::kUseCustomChromeFrame,
                    !prefs->GetBoolean(prefs::kUseCustomChromeFrame));
              },
              profile))
          .SetActionId(kUseSystemTitleBar)
          .Build());
#endif  // BUILDFLAG(IS_LINUX)

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CloseTabSearch(bwi);
              },
              bwi))
          .SetActionId(kActionTabSearchClose)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ExecuteUIDebugCommand(IDC_DEBUG_TOGGLE_TABLET_MODE,
                                              bwi);
              },
              bwi))
          .SetActionId(kActionDebugToggleTabletMode)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ExecuteUIDebugCommand(IDC_DEBUG_PRINT_VIEW_TREE, bwi);
              },
              bwi))
          .SetActionId(kActionDebugPrintViewTree)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ExecuteUIDebugCommand(IDC_DEBUG_PRINT_VIEW_TREE_DETAILS,
                                              bwi);
              },
              bwi))
          .SetActionId(kActionDebugPrintViewTreeDetails)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CloseWindow(bwi);
              },
              bwi))
          .SetActionId(kActionCloseWindow)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleFullscreenMode(bwi);
              },
              bwi))
          .SetActionId(kActionFullscreen)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::AttemptUserExit();
              },
              bwi))
          .SetActionId(kActionExit)
          .Build());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetWindow()->Restore();
              },
              bwi))
          .SetActionId(kRestoreWindow)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetWindow()->Minimize();
              },
              bwi))
          .SetActionId(kActionMinimizeWindow)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                bwi->GetWindow()->Maximize();
              },
              bwi))
          .SetActionId(kActionMaximizeWindow)
          .Build());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::PromptToNameWindow(bwi);
              },
              bwi))
          .SetActionId(kActionNameWindow)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                web_app::ReparentWebAppForActiveTab(
                    bwi->GetBrowserForMigrationOnly());
              },
              bwi))
          .SetActionId(kActionOpenInPwaWindow)
          .Build());

#if BUILDFLAG(IS_MAC)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleAlwaysShowToolbarInFullscreen(
                    bwi->GetBrowserForMigrationOnly());
              },
              bwi))
          .SetActionId(kActionToggleFullscreenToolbar)
          .Build());
#endif  // BUILDFLAG(IS_MAC)

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::MuteSite(bwi);
              },
              bwi))
          .SetActionId(kActionWindowMuteSite)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CloseTab(bwi);
              },
              bwi))
          .SetActionId(kActionCloseTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNextTab(bwi);
              },
              bwi))
          .SetActionId(kActionSelectNextTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectPreviousTab(bwi);
              },
              bwi))
          .SetActionId(kActionSelectPreviousTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 0);
              },
              bwi))
          .SetActionId(kActionSelectTab0)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 1);
              },
              bwi))
          .SetActionId(kActionSelectTab1)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 2);
              },
              bwi))
          .SetActionId(kActionSelectTab2)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 3);
              },
              bwi))
          .SetActionId(kActionSelectTab3)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 4);
              },
              bwi))
          .SetActionId(kActionSelectTab4)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 5);
              },
              bwi))
          .SetActionId(kActionSelectTab5)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 6);
              },
              bwi))
          .SetActionId(kActionSelectTab6)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectNumberedTab(bwi, 7);
              },
              bwi))
          .SetActionId(kActionSelectTab7)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SelectLastTab(bwi);
              },
              bwi))
          .SetActionId(kActionSelectLastTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::DuplicateTab(bwi);
              },
              bwi))
          .SetActionId(kActionDuplicateTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::RestoreTab(bwi);
              },
              bwi))
          .SetActionId(kActionRestoreTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ConvertPopupToTabbedBrowser(bwi);
              },
              bwi))
          .SetActionId(kActionShowAsTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::MoveTabNext(bwi);
              },
              bwi))
          .SetActionId(kActionMoveTabNext)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::MoveTabPrevious(bwi);
              },
              bwi))
          .SetActionId(kActionMoveTabPrevious)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::MoveActiveTabToNewWindow(bwi);
              },
              bwi))
          .SetActionId(kActionMoveTabToNewWindow)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::BookmarkAllTabs(bwi);
              },
              bwi))
          .SetActionId(kActionBookmarkAllTabs)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::PinTab(bwi);
              },
              bwi))
          .SetActionId(kActionWindowPinTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::GroupTab(bwi);
              },
              bwi))
          .SetActionId(kActionWindowGroupTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CloseTabsToRight(bwi);
              },
              bwi))
          .SetActionId(kActionWindowCloseTabsToRight)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CloseOtherTabs(bwi);
              },
              bwi))
          .SetActionId(kActionWindowCloseOtherTabs)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::NewTabToRight(bwi);
              },
              bwi))
          .SetActionId(kActionNewTabToRight)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::AddNewTabToGroup(bwi);
              },
              bwi))
          .SetActionId(kActionAddNewTabToGroup)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CreateNewTabGroup(bwi);
              },
              bwi))
          .SetActionId(kActionCreateNewTabGroup)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusNextTabGroup(bwi);
              },
              bwi))
          .SetActionId(kActionFocusNextTabGroup)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusPreviousTabGroup(bwi);
              },
              bwi))
          .SetActionId(kActionFocusPrevTabGroup)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CloseTabGroup(bwi);
              },
              bwi))
          .SetActionId(kActionCloseTabGroup)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::GroupAllUngroupedTabs(bwi);
              },
              bwi))
          .SetActionId(kActionGroupUngroupedTabs)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::CreateNewTabGroup(bwi);
              },
              bwi))
          .SetActionId(kActionCreateNewTabGroupTopLevel)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::AddNewTabToRecentGroup(bwi);
              },
              bwi))
          .SetActionId(kActionAddNewTabRecentGroup)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::PinKeyboardFocusedTab(bwi);
              },
              bwi))
          .SetActionId(kActionPinTargetTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::GroupKeyboardFocusedTab(bwi);
              },
              bwi))
          .SetActionId(kActionGroupTargetTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::DuplicateKeyboardFocusedTab(bwi);
              },
              bwi))
          .SetActionId(kActionDuplicateTargetTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleRequestTabletSite(bwi);
              },
              bwi))
          .SetActionId(kActionToggleRequestTabletSite)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::MoveCurrentTabToReadLater(bwi);
              },
              bwi))
          .SetActionId(kActionReadingListMenuAddTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowSettingsSubPage(bwi, chrome::kPeopleSubPage);
              },
              bwi))
          .SetActionId(kActionRecentTabsLoginForDeviceTabs)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowHistorySubPage(bwi,
                                           chrome::kChromeUIHistorySyncedTabs);
              },
              bwi))
          .SetActionId(kActionRecentTabsSeeDeviceTabs)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusToolbar(bwi);
              },
              bwi))
          .SetActionId(kActionFocusToolbar)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusLocationBar(bwi);
              },
              bwi))
          .SetActionId(kActionFocusLocation)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusSearch(bwi);
              },
              bwi))
          .SetActionId(kActionFocusSearch)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusAppMenu(bwi);
              },
              bwi))
          .SetActionId(kActionFocusMenuBar)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusNextPane(bwi);
              },
              bwi))
          .SetActionId(kActionFocusNextPane)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusPreviousPane(bwi);
              },
              bwi))
          .SetActionId(kActionFocusPreviousPane)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusBookmarksToolbar(bwi);
              },
              bwi))
          .SetActionId(kActionFocusBookmarks)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusInactivePopupForAccessibility(bwi);
              },
              bwi))
          .SetActionId(kActionFocusInactivePopupForAccessibility)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusWebContentsPane(bwi);
              },
              bwi))
          .SetActionId(kActionFocusWebContentsPane)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::Zoom(bwi, content::PAGE_ZOOM_IN);
              },
              bwi))
          .SetActionId(kActionZoomPlus)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::Zoom(bwi, content::PAGE_ZOOM_OUT);
              },
              bwi))
          .SetActionId(kActionZoomMinus)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ExclusiveAccessManager* manager =
                    ExclusiveAccessManager::From(bwi);
                if (manager) {
                  manager->ExitExclusiveAccess();
                }
              },
              bwi))
          .SetActionId(kActionContentContextExitFullscreen)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                NavigateToManagePasswordsPage(
                    bwi, password_manager::ManagePasswordsReferrer::
                             kPasswordContextMenu);
              },
              bwi))
          .SetActionId(kActionContentContextShowAllSavedPasswords)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleBookmarkBarWhenVisible(bwi->GetProfile());
              },
              bwi))
          .SetActionId(kActionBookmarkBarAlwaysShow)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                PrefService* prefs = bwi->GetProfile()->GetPrefs();
                prefs->SetBoolean(
                    bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                    !prefs->GetBoolean(
                        bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
              },
              bwi))
          .SetActionId(kActionBookmarkBarShowAppsShortcut)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                PrefService* prefs = bwi->GetProfile()->GetPrefs();
                prefs->SetBoolean(
                    bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
                    !prefs->GetBoolean(
                        bookmarks::prefs::kShowManagedBookmarksInBookmarkBar));
              },
              bwi))
          .SetActionId(kActionBookmarkBarShowManagedBookmarks)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                BookmarkUndoServiceFactory::GetForProfile(bwi->GetProfile())
                    ->undo_manager()
                    ->Undo();
              },
              bwi))
          .SetActionId(kActionBookmarkBarUndo)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                BookmarkUndoServiceFactory::GetForProfile(bwi->GetProfile())
                    ->undo_manager()
                    ->Redo();
              },
              bwi))
          .SetActionId(kActionBookmarkBarRedo)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowBookmarkManager(bwi);
              },
              bwi))
          .SetActionId(kActionBookmarkManager)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                PrefService* service = g_browser_process->local_state();
                if (service) {
                  service->SetBoolean(prefs::kBackgroundModeEnabled, false);
                }
              },
              bwi))
          .SetActionId(kActionStatusTrayKeepChromeRunningInBackground)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* const active_tab =
                    bwi->GetActiveTabInterface();
                if (!active_tab) {
                  return;
                }
                content::WebContents* const web_contents =
                    active_tab->GetContents();
                if (!web_contents) {
                  return;
                }
                ChromeTranslateClient* chrome_translate_client =
                    ChromeTranslateClient::FromWebContents(web_contents);
                if (!chrome_translate_client) {
                  return;
                }
                translate::TranslateManager* manager =
                    chrome_translate_client->GetTranslateManager();
                if (manager) {
                  manager->ShowTranslateUI(/*auto_translate=*/true,
                                           /*triggered_from_menu=*/true);
                }
              },
              bwi))
          .SetActionId(kActionContentContextTranslate)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                read_anything::ReadAnythingEntryPointController::ShowUI(
                    bwi, ReadAnythingOpenTrigger::kReadAnythingContextMenu);
              },
              bwi))
          .SetActionId(kActionContentContextOpenInReadingMode)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                read_anything::ReadAnythingEntryPointController::ShowUI(
                    bwi, ReadAnythingOpenTrigger::kReadAnythingContextMenu);
              },
              bwi))
          .SetActionId(kActionContentContextListenToThisPage)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* const active_tab =
                    bwi->GetActiveTabInterface();
                if (!active_tab) {
                  return;
                }
                content::WebContents* const web_contents =
                    active_tab->GetContents();
                if (!web_contents) {
                  return;
                }
                if (lens::features::IsLensOverlayEnabled()) {
                  LensSearchController* const controller =
                      LensSearchController::FromTabWebContents(web_contents);
                  if (controller) {
                    controller->OpenLensOverlay(
                        lens::LensOverlayInvocationSource::
                            kContentAreaContextMenuPage);
                    return;
                  }
                }
                chrome::ExecLensRegionSearch(bwi);
              },
              bwi))
          .SetActionId(kActionContentContextLensRegionSearch)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* const active_tab =
                    bwi->GetActiveTabInterface();
                if (!active_tab) {
                  return;
                }
                content::WebContents* const web_contents =
                    active_tab->GetContents();
                if (!web_contents) {
                  return;
                }
                if (base::FeatureList::IsEnabled(
                        features::kDevToolsShowPolicyDialog) &&
                    !DevToolsWindow::AllowDevToolsFor(bwi->GetProfile(),
                                                      web_contents)) {
                  DevToolsPolicyDialog::Show(web_contents);
                } else {
                  web_contents->GetPrimaryMainFrame()->ViewSource();
                }
              },
              bwi))
          .SetActionId(kActionViewSource)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SavePage(bwi);
              },
              bwi))
          .SetActionId(kActionSavePage)
          .Build());

#if BUILDFLAG(ENABLE_PRINTING)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::BasicPrint(bwi);
              },
              bwi))
          .SetActionId(kActionBasicPrint)
          .Build());
#endif  // BUILDFLAG(ENABLE_PRINTING)

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* const active_tab =
                    bwi->GetActiveTabInterface();
                if (active_tab && active_tab->GetContents()) {
                  active_tab->GetContents()->Focus();
                }
              },
              bwi))
          .SetActionId(kActionFocusThisTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::MuteSiteForKeyboardFocusedTab(bwi);
              },
              bwi))
          .SetActionId(kActionMuteTargetSite)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FindNext(bwi);
              },
              bwi))
          .SetActionId(kActionFindNext)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FindPrevious(bwi);
              },
              bwi))
          .SetActionId(kActionFindPrevious)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (chrome::CanCloseFind(bwi)) {
                  chrome::CloseFind(bwi);
                } else {
                  chrome::Stop(bwi);
                }
              },
              bwi))
          .SetActionId(kActionCloseFindOrStop)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
                chrome::CreateDesktopShortcutForActiveWebContents(
                    bwi->GetBrowserForMigrationOnly());
#else
                web_app::CreateWebAppFromCurrentWebContents(
                    bwi->GetBrowserForMigrationOnly(),
                    web_app::WebAppInstallFlow::kCreateShortcut);
#endif
              },
              bwi))
          .SetActionId(kActionCreateShortcut)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleDevToolsWindow(
                    bwi, DevToolsToggleAction::ShowConsolePanel(),
                    DevToolsOpenedByAction::kConsoleShortcut);
              },
              bwi))
          .SetActionId(kActionDevToolsConsole)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleDevToolsWindow(
                    bwi, DevToolsToggleAction::Toggle(),
                    DevToolsOpenedByAction::kToggleShortcut);
              },
              bwi))
          .SetActionId(kActionDevToolsToggle)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleShowSearchTools(bwi);
              },
              bwi))
          .SetActionId(kActionShowSearchTools)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleCaretBrowsing(bwi);
              },
              bwi))
          .SetActionId(kActionCaretBrowsingToggle)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* const active_tab =
                    bwi->GetActiveTabInterface();
                if (!active_tab) {
                  return;
                }
                content::WebContents* const web_contents =
                    active_tab->GetContents();
                if (!web_contents) {
                  return;
                }
                auto* bubble_controller =
                    qrcode_generator::QRCodeGeneratorBubbleController::Get(
                        web_contents);
                if (bubble_controller) {
                  base::RecordAction(base::UserMetricsAction(
                      "SharingQRCode.DialogLaunched.ContextMenuPage"));
                  bubble_controller->ShowBubble(
                      web_contents->GetLastCommittedURL());
                }
              },
              bwi))
          .SetActionId(kActionContentContextGenerateQrCode)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowSettings(bwi);
              },
              bwi))
          .SetActionId(kActionOptions)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowImportDialog(bwi);
              },
              bwi))
          .SetActionId(kActionImportSettings)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowSearchEngineSettings(bwi);
              },
              bwi))
          .SetActionId(kActionEditSearchEngines)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                NavigateToManagePasswordsPage(
                    bwi,
                    password_manager::ManagePasswordsReferrer::kChromeMenuItem);
              },
              bwi))
          .SetActionId(kActionViewPasswords)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowEnterpriseManagementPageInTabbedBrowser(bwi);
              },
              bwi))
          .SetActionId(kActionShowManagementPage)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Profile* profile = bwi->GetProfile();
                if (profile->IsIncognitoProfile()) {
                  chrome::CloseAllBrowsersWithIncognitoProfile(profile);
                } else {
                  profiles::CloseProfileWindows(profile);
                }
              },
              bwi))
          .SetActionId(kActionCloseProfile)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Profile* profile = bwi->GetProfile();
                signin::IdentityManager* identity_manager =
                    IdentityManagerFactory::GetForProfileIfExists(profile);
                if (identity_manager && identity_manager->HasPrimaryAccount(
                                            signin::ConsentLevel::kSignin)) {
                  std::string email =
                      identity_manager
                          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                          .email;
                  GURL google_account = net::AppendQueryParameter(
                      GURL(chrome::kGoogleAccountURL), "utm_source",
                      "chrome-profile-chooser");
                  GURL url(chrome::kGoogleAccountChooserURL);
                  url = net::AppendQueryParameter(url, "Email", email);
                  url = net::AppendQueryParameter(url, "continue",
                                                  google_account.spec());
                  ::ShowSingletonTab(bwi, url);
                }
              },
              bwi))
          .SetActionId(kActionManageGoogleAccount)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowSettingsSubPage(bwi, chrome::kSyncSetupSubPage);
              },
              bwi))
          .SetActionId(kActionShowSyncSettings)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Profile* profile = bwi->GetProfile();
                signin::IdentityManager* identity_manager =
                    IdentityManagerFactory::GetForProfileIfExists(profile);
                AccountInfo account_info;
                if (identity_manager) {
                  CoreAccountInfo account =
                      identity_manager->GetPrimaryAccountInfo(
                          signin::ConsentLevel::kSignin);
                  account_info =
                      identity_manager->FindExtendedAccountInfo(account);
                }
                signin_ui_util::EnableSyncFromSingleAccountPromo(
                    profile, account_info, signin_metrics::AccessPoint::kMenu);
              },
              bwi))
          .SetActionId(kActionTurnOnSync)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Profile* profile = bwi->GetProfile();
                signin::IdentityManager* identity_manager =
                    IdentityManagerFactory::GetForProfileIfExists(profile);
                AccountInfo account_info;
                if (identity_manager) {
                  CoreAccountInfo account =
                      identity_manager->GetPrimaryAccountInfo(
                          signin::ConsentLevel::kSignin);
                  account_info =
                      identity_manager->FindExtendedAccountInfo(account);
                }
                signin_ui_util::SignInFromSingleAccountPromo(
                    profile, account_info, signin_metrics::AccessPoint::kMenu);
              },
              bwi))
          .SetActionId(kActionShowSignin)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Profile* profile = bwi->GetProfile();
                signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
                    profile, signin_metrics::AccessPoint::kMenu);
              },
              bwi))
          .SetActionId(kActionShowSigninWhenPaused)
          .Build());

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
                    ->StartSetAsDefault(base::DoNothing());
              },
              bwi))
          .SetActionId(kActionSetBrowserAsDefault)
          .Build());
#endif

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleShowFullURLs(bwi);
              },
              bwi))
          .SetActionId(kActionShowFullUrls)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleShowGoogleLensShortcut(bwi);
              },
              bwi))
          .SetActionId(kActionShowGoogleLensShortcut)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleShowAiModeOmniboxButton(bwi);
              },
              bwi))
          .SetActionId(kActionShowAiModeOmniboxButton)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                PrefService* pref_service = bwi->GetProfile()->GetPrefs();
                const char* pref_name =
                    media_router::prefs::
                        kMediaRouterShowCastSessionsStartedByOtherDevices;
                pref_service->SetBoolean(pref_name,
                                         !pref_service->GetBoolean(pref_name));
              },
              bwi))
          .SetActionId(kActionMediaToolbarContextShowOtherSessions)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                PrefService* prefs = bwi->GetProfile()->GetPrefs();
                const char* pref_name =
                    "accessibility.captions.live_caption_enabled";
                bool is_enabled = !prefs->GetBoolean(pref_name);
                prefs->SetBoolean(pref_name, is_enabled);
                base::UmaHistogramBoolean(
                    "Accessibility.LiveCaption.EnableFromContextMenu",
                    is_enabled);
              },
              bwi))
          .SetActionId(kActionLiveCaption)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::FocusLocationBar(bwi);
              },
              bwi))
          .SetActionId(kActionSearch)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowSettingsSubPage(bwi, chrome::kManageProfileSubPage);
              },
              bwi))
          .SetActionId(kActionCustomizeChrome)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleBookmarkBar(bwi);
              },
              bwi))
          .SetActionId(kActionShowBookmarkBar)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowBookmarkManager(bwi);
              },
              bwi))
          .SetActionId(kActionShowBookmarkManager)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowAboutChrome(bwi);
              },
              bwi))
          .SetActionId(kActionAbout)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::OpenInChrome(bwi);
              },
              bwi))
          .SetActionId(kActionOpenInChrome)
          .Build());

#if !BUILDFLAG(IS_CHROMEOS)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Browser* browser = bwi->GetBrowserForMigrationOnly();
                auto* controller = web_app::AppBrowserController::From(browser);
                if (controller) {
                  chrome::ShowWebAppSettings(
                      bwi, controller->app_id(),
                      web_app::AppSettingsPageEntryPoint::kBrowserCommand);
                }
              },
              bwi))
          .SetActionId(kActionWebAppSettings)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating([](actions::ActionItem* item,
                                 actions::ActionInvocationContext context) {
            ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
                ProfilePicker::EntryPoint::
                    kAppMenuProfileSubMenuAddNewProfile));
          }))
          .SetActionId(kActionAddNewProfile)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating([](actions::ActionItem* item,
                                 actions::ActionInvocationContext context) {
            ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
                ProfilePicker::EntryPoint::
                    kAppMenuProfileSubMenuManageProfiles));
          }))
          .SetActionId(kActionManageChromeProfiles)
          .Build());
#endif

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SaveAutofillAddress(bwi);
              },
              bwi))
          .SetActionId(kActionSaveAutofillAddress)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::SharingHub(bwi);
              },
              bwi))
          .SetActionId(kActionSharingHub)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ScreenshotCapture(bwi);
              },
              bwi))
          .SetActionId(kActionSharingHubScreenshot)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowFeedbackPage(
                    bwi, feedback::kFeedbackSourceBrowserCommand, std::string(),
                    std::string(), std::string(), std::string());
              },
              bwi))
          .SetActionId(kActionFeedback)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowHistory(bwi);
              },
              bwi))
          .SetActionId(kActionShowHistory)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowHelp(bwi, chrome::HelpSource::kKeyboard);
              },
              bwi))
          .SetActionId(kActionHelpPageViaKeyboard)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowHelp(bwi, chrome::HelpSource::kMenu);
              },
              bwi))
          .SetActionId(kActionHelpPageViaMenu)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowAppMenu(bwi);
              },
              bwi))
          .SetActionId(kActionShowAppMenu)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowExtensions(bwi);
              },
              bwi))
          .SetActionId(kActionManageExtensions)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::OpenUpdateChromeDialog(bwi);
              },
              bwi))
          .SetActionId(kActionUpgradeDialog)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowAvatarMenu(bwi);
              },
              bwi))
          .SetActionId(kActionShowAvatarMenu)
          .Build());

#if BUILDFLAG(IS_CHROMEOS)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ::TakeScreenshot();
              },
              bwi))
          .SetActionId(kActionTakeScreenshot)
          .Build());
#endif

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowBetaForum(bwi);
              },
              bwi))
          .SetActionId(kActionShowBetaForum)
          .Build());

#if BUILDFLAG(IS_MAC)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                tabs::TabInterface* const active_tab =
                    bwi->GetActiveTabInterface();
                if (!active_tab) {
                  return;
                }
                content::WebContents* const web_contents =
                    active_tab->GetContents();
                if (!web_contents) {
                  return;
                }
                if (base::FeatureList::IsEnabled(
                        features::kDevToolsShowPolicyDialog) &&
                    !DevToolsWindow::AllowDevToolsFor(bwi->GetProfile(),
                                                      web_contents)) {
                  DevToolsPolicyDialog::Show(web_contents);
                } else {
                  chrome::ToggleJavaScriptFromAppleEventsAllowed(
                      bwi->GetBrowserForMigrationOnly());
                }
              },
              bwi))
          .SetActionId(kActionToggleJavascriptAppleEvents)
          .Build());
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowChromeTips(bwi);
              },
              bwi))
          .SetActionId(kActionChromeTips)
          .Build());
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowChromeWhatsNew(bwi);
              },
              bwi))
          .SetActionId(kActionChromeWhatsNew)
          .Build());
#endif

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowSettingsSubPage(bwi, chrome::kPerformanceSubPage);
              },
              bwi))
          .SetActionId(kActionPerformance)
          .Build());

#if BUILDFLAG(ENABLE_SPELLCHECK) && !BUILDFLAG(IS_MAC)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Profile* const profile = bwi->GetProfile();
                if (!profile) {
                  return;
                }
                PrefService* prefs = profile->GetPrefs();
                bool spellcheck_enabled =
                    prefs->GetBoolean(spellcheck::prefs::kSpellCheckEnable);
                bool enhanced_spellcheck_enabled = prefs->GetBoolean(
                    spellcheck::prefs::kSpellCheckUseSpellingService);

                if (spellcheck_enabled && !enhanced_spellcheck_enabled) {
                  // User is turning off spell check.
                  prefs->SetBoolean(spellcheck::prefs::kSpellCheckEnable,
                                    false);
                } else if (enhanced_spellcheck_enabled) {
                  // User is choosing 'basic' over 'enhanced'.
                  prefs->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
                  prefs->SetBoolean(
                      spellcheck::prefs::kSpellCheckUseSpellingService, false);
                } else {
                  // User is turning on spell check.
                  prefs->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
                }
              },
              bwi))
          .SetText(l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_CHECK_SPELLING_WHILE_TYPING))
          .SetActionId(kActionCheckSpellingWhileTyping)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                Profile* const profile = bwi->GetProfile();
                if (!profile) {
                  return;
                }
                std::vector<SpellcheckService::Dictionary> dictionaries;
                SpellcheckService::GetDictionaries(profile, &dictionaries);

                std::vector<std::string> all_languages;
                for (const auto& dictionary : dictionaries) {
                  all_languages.push_back(dictionary.language);
                }

                StringListPrefMember dictionaries_pref;
                dictionaries_pref.Init(
                    spellcheck::prefs::kSpellCheckDictionaries,
                    profile->GetPrefs());
                dictionaries_pref.SetValue(all_languages);
              },
              bwi))
          .SetText(l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_SPELLCHECK_MULTI_LINGUAL))
          .SetActionId(kActionSpellcheckMultiLingual)
          .Build());
#endif
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableResurrectingPaymentsUsers)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  if (!bwi) {
                    return;
                  }

                  auto* tab = bwi->GetActiveTabInterface();
                  if (!tab) {
                    return;
                  }

                  if (auto* controller =
                          autofill::PaymentsChurnedUsersBubbleController::From(
                              *tab)) {
                    controller->ReshowBubble();
                  }
                },
                bwi))
            .SetActionId(kActionShowPaymentsChurnedUsersBubble)
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? kCreditCardIcon
                    : kCreditCardChromeRefreshOldIcon))
            .Build());
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void BrowserActions::AddListeners() {
  browser_action_prefs_listener_ = std::make_unique<BrowserActionPrefsListener>(
      base::to_address(profile_), this);
}

void BrowserActions::InitializeNavigationActions() {
  BrowserWindowInterface* const bwi = base::to_address(bwi_);

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                WindowOpenDisposition disposition =
                    context.GetProperty(chrome::kDispositionKey);
                chrome::GoBack(bwi, disposition);
              },
              bwi))
          .SetActionId(kActionBack)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                WindowOpenDisposition disposition =
                    context.GetProperty(chrome::kDispositionKey);
                chrome::Reload(bwi, disposition);
              },
              bwi))
          .SetActionId(kActionReload)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::Stop(bwi);
              },
              bwi))
          .SetActionId(kActionStop)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                WindowOpenDisposition disposition =
                    context.GetProperty(chrome::kDispositionKey);
                chrome::ClearCache(bwi);
                chrome::ReloadBypassingCache(bwi, disposition);
              },
              bwi))
          .SetActionId(kActionReloadClearingCache)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                WindowOpenDisposition disposition =
                    context.GetProperty(chrome::kDispositionKey);
                chrome::ReloadBypassingCache(bwi, disposition);
              },
              bwi))
          .SetActionId(kActionReloadBypassingCache)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::OpenCurrentURL(bwi);
              },
              bwi))
          .SetActionId(kActionOpenCurrentUrl)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                WindowOpenDisposition disposition =
                    context.GetProperty(chrome::kDispositionKey);
                chrome::Home(bwi, disposition);
              },
              bwi))
          .SetActionId(kActionHome)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                WindowOpenDisposition disposition =
                    context.GetProperty(chrome::kDispositionKey);
                chrome::GoForward(bwi, disposition);
              },
              bwi))
          .SetActionId(kActionForward)
          .Build());
}
