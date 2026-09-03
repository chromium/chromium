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
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/autofill/payments/payments_churned_users_bubble_controller.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
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
#include "chrome/browser/geic/geic_enabling.h"
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
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
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
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_action_prefs_listener.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_select_file_dialog_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
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
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
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
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
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
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/bookmarks/common/bookmark_bar_visibility_state.h"
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
#include "content/public/browser/page_navigator.h"
#include "content/public/common/profiling.h"
#include "extensions/common/extension_urls.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/browser_commands_chromeos.h"
#endif
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "components/lens/lens_overlay_invocation_source.h"
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

ui::Accelerator GetAcceleratorForCommandId(int command_id) {
  ui::Accelerator accelerator;
  if (::GetAcceleratorForCommandId(command_id, &accelerator)) {
    return accelerator;
  }
  return ui::Accelerator();
}

actions::ActionItem::ActionItemBuilder ChromeMenuAction(
    actions::ActionItem::InvokeActionCallback callback,
    actions::ActionId action_id,
    int title_id,
    int tooltip_id,
    const gfx::VectorIcon& icon,
    bool is_pinnable = true) {
  auto builder =
      actions::ActionItem::Builder(callback)
          .SetActionId(action_id)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(title_id)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(tooltip_id)))
          .SetImage(ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon));
  if (is_pinnable) {
    builder.SetProperty(actions::kActionItemPinnableKey,
                        std::underlying_type_t<actions::ActionPinnableState>(
                            actions::ActionPinnableState::kPinnable));
  }
  return builder;
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

DEFINE_USER_DATA(BrowserActions);

// static
BrowserActions* BrowserActions::From(BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// static
const BrowserActions* BrowserActions::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

BrowserActions::BrowserActions(BrowserWindowInterface* bwi)
    : bwi_(CHECK_DEREF(bwi)),
      profile_(CHECK_DEREF(bwi->GetProfile())),
      scoped_unowned_user_data_(bwi->GetUnownedUserDataHost(), *this) {}

BrowserActions::~BrowserActions() {
  browser_action_prefs_listener_.reset();
  // Extract the root and destruct it after the raw_ptr to avoid a dangling
  // pointer scenario.
  if (root_action_item_) {
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

  InitializeSubmenuActions();

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
                      features::IsRoundedIconsEnabled()
                          ? kEditIcon
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
              features::IsRoundedIconsEnabled() ? kMenuBookIcon
                                                : kMenuBookChromeRefreshOldIcon,
              ui::kColorIcon))
          .SetProperty(
              actions::kActionItemPinnableKey,
              static_cast<std::underlying_type_t<actions::ActionPinnableState>>(
                  actions::ActionPinnableState::kPinnable))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                read_anything::ReadAnythingEntryPointController::ToggleUI(
                    bwi, read_anything::mojom::ReadAnythingOpenTrigger::
                             kKeyboardShortcut);
              },
              bwi))
          .SetActionId(kActionShowReadingModeKeyboard)
          .SetText(l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE))
          .SetTooltipText(l10n_util::GetStringFUTF16(IDS_READING_MODE_TOOLTIP,
                                                     reading_mode_shortcut))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kMenuBookIcon
                                                : kMenuBookChromeRefreshOldIcon,
              ui::kColorIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                std::underlying_type_t<SidePanelOpenTrigger>
                    side_panel_trigger =
                        context.GetProperty(kSidePanelOpenTriggerKey);
                read_anything::mojom::ReadAnythingOpenTrigger open_trigger =
                    read_anything::mojom::ReadAnythingOpenTrigger::kAppMenu;
                if (side_panel_trigger != -1) {
                  std::optional<read_anything::mojom::ReadAnythingOpenTrigger>
                      mapped_trigger =
                          read_anything::SidePanelToReadAnythingOpenTrigger(
                              static_cast<SidePanelOpenTrigger>(
                                  side_panel_trigger));
                  if (mapped_trigger.has_value()) {
                    open_trigger = mapped_trigger.value();
                  }
                }
                read_anything::ReadAnythingEntryPointController::ShowUI(
                    bwi, open_trigger);
              },
              bwi))
          .SetActionId(kActionShowReadingModeSidePanel)
          .SetText(l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE))
          .SetTooltipText(l10n_util::GetStringFUTF16(IDS_READING_MODE_TOOLTIP,
                                                     reading_mode_shortcut))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kMenuBookIcon
                                                : kMenuBookChromeRefreshOldIcon,
              ui::kColorIcon))
          .Build());

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
                lens::GetLensOverlayEntrypointLabelAltIds()))
            .SetTooltipText(l10n_util::GetStringUTF16(
                lens::GetLensOverlayEntrypointLabelAltIds()))
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

  if (contextual_tasks::IsContextualTasksUIEnabled()) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  if (!bwi) {
                    return;
                  }
                  auto* controller =
                      contextual_tasks::ContextualTasksPanelController::From(
                          bwi);
                  if (controller) {
                    bool is_open = controller->IsPanelOpenForContextualTask();
                    const char* user_action =
                        is_open ? "ContextualTasks.PermanentToolbarButton."
                                  "UserAction."
                                  "CloseSidePanel"
                                : "ContextualTasks.PermanentToolbarButton."
                                  "UserAction."
                                  "OpenSidePanel";
                    base::RecordAction(base::UserMetricsAction(user_action));
                    base::UmaHistogramBoolean(user_action, true);
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
            .SetVisible(contextual_tasks::EntryPointEligibilityManager::
                            IsPinningEligible(profile))
            .Build());
  }

  if (geic::IsGeicEnabled(profile)) {
    root_action_item_->AddChild(
        SidePanelAction(
            SidePanelEntryId::kGeic, IDS_SETTINGS_SIDE_PANEL_ALIGNMENT_GLIC,
            IDS_SETTINGS_SIDE_PANEL_ALIGNMENT_GLIC, omnibox::kSparkIcon,
            kActionSidePanelShowGeic, bwi, false)
            .SetVisible(true)
            .Build());
  } else if (glic::GlicEnabling::IsEnabledByGlobalCriteria()) {
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
                  ? vector_icons::kShoppingmodeIcon
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
                    memory_saver::MemorySaverBubbleController::From(bwi);
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
                    CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi))
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
          .SetActionId(kActionShowZoomBubble)
          .SetText(l10n_util::GetStringUTF16(IDS_ZOOM_NORMAL))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_ZOOM))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kZoomInIcon : kZoomInOldIcon))
          .SetEnabled(true)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::Find(bwi);
              },
              bwi))
          .SetActionId(kActionFind)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_FIND)))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FIND))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? omnibox::kFindInPageIcon
                  : omnibox::kFindInPageChromeRefreshOldIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_FIND))
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
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_FALLBACK_ICON_TOOLTIP))
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
          .SetProperty(actions::kShortTitleTextKey,
                       new std::u16string(
                           l10n_util::GetStringUTF16(IDS_APP_MENU_INCOGNITO)))
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

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleVerticalTabs(bwi);
              },
              bwi))
          .SetActionId(kActionToggleVerticalTabs)
          .SetText(l10n_util::GetStringUTF16(IDS_SWITCH_TO_VERTICAL_TAB))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_SWITCH_TO_VERTICAL_TAB))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kDockToRightIcon
                                                : kDockToLeftOldIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleVerticalTabsExpandOnHover(bwi);
              },
              bwi))
          .SetActionId(kActionToggleVerticalTabsExpandOnHover)
          .SetText(l10n_util::GetStringUTF16(
              IDS_VERTICAL_TABS_ENABLE_EXPAND_ON_HOVER))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                auto* controller =
                    tabs::VerticalTabStripStateController::From(bwi);
                if (!controller) {
                  // The controller is only instantiated for normal browsers.
                  return;
                }
                bool collapse = controller->GetCollapseState() ==
                                tabs::VerticalTabStripCollapseState::kExpanded;
                controller->RequestCollapse(collapse);
                if (context.GetProperty(chrome::kActionInvocationSourceKey) ==
                    chrome::ActionInvocationSource::kKeyboardShortcut) {
                  base::RecordAction(base::UserMetricsAction(
                      collapse ? "VerticalTabs_TabStrip_"
                                 "KeyboardShortcutToggleCollapsed"
                               : "VerticalTabs_TabStrip_"
                                 "KeyboardShortcutToggleUncollapsed"));
                } else {
                  base::RecordAction(base::UserMetricsAction(
                      collapse
                          ? "VerticalTabs_TabStrip_ButtonToggleCollapsed"
                          : "VerticalTabs_TabStrip_ButtonToggleUncollapsed"));
                }
              },
              bwi))
          .SetActionId(kActionToggleCollapseVertical)
          .SetAccelerator(ui::Accelerator(
              ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR))
          .SetEnabled(false)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowFeedbackPage(bwi,
                                         feedback::kFeedbackSourceVerticalTabs,
                                         /*description_template=*/"",
                                         /*description_placeholder_text=*/"",
                                         /*category_tag=*/"vertical_tabs",
                                         /*extra_diagnostics=*/"");
              },
              bwi))
          .SetActionId(kActionVerticalTabsSendFeedback)
          .SetText(l10n_util::GetStringUTF16(IDS_VERTICAL_TABS_SEND_FEEDBACK))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kFeedbackIcon
                  : vector_icons::kFeedbackOldIcon,
              ui::kColorIcon))
          .Build());

  if (organizer_panel::IsOrganizerPanelFeatureEnabled()) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* controller = OrganizerPanelStateController::From(bwi);
                  if (controller) {
                    controller->SetOrganizerVisible(
                        !controller->IsOrganizerPanelVisible());
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
            .SetActionId(kActionToggleOrganizerPanel)
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
                  ? vector_icons::kAddWeight500CustomIcon
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
          .SetAccelerator(GetAcceleratorForCommandId(IDC_PRINT))
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
                    webui::GetBrowserForOpeningWebUi(bwi);
                if (is_incognito) {
                  chrome::ShowIncognitoClearBrowsingDataDialog(
                      browser_for_opening_webui);
                } else {
                  chrome::ShowClearBrowsingDataDialog(
                      browser_for_opening_webui);
                }
#if !BUILDFLAG(IS_ANDROID)
                ui::ElementContext browser_element_context =
                    BrowserElements::From(bwi)->GetContext();
                ui::TrackedElement* const tracked_element =
                    ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                        kBrowserViewElementId, browser_element_context);
                if (tracked_element) {
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      tracked_element, browsing_data_important_sites_util::
                                           kShowClearBrowsingDataDialogEventId);
                }
#endif  // !BUILDFLAG(IS_ANDROID)
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
            vector_icons::kTableChartIcon)
            .Build());

    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::OpenTaskManager(
                      bwi, task_manager::StartAction::kMoreTools);
                },
                bwi),
            kActionTaskManagerAppMenu, IDS_TASK_MANAGER, IDS_TASK_MANAGER,
            vector_icons::kTableChartIcon,
            /*is_pinnable=*/false)
            .Build());
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::OpenTaskManager(bwi,
                                          task_manager::StartAction::kShortcut);
                },
                bwi),
            kActionTaskManagerShortcut, IDS_TASK_MANAGER, IDS_TASK_MANAGER,
            vector_icons::kTableChartIcon,
            /*is_pinnable=*/false)
            .Build());
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::OpenTaskManager(
                      bwi, task_manager::StartAction::kContextMenu);
                },
                bwi),
            kActionTaskManagerContextMenu, IDS_TASK_MANAGER, IDS_TASK_MANAGER,
            vector_icons::kTableChartIcon,
            /*is_pinnable=*/false)
            .Build());
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::OpenTaskManager(bwi,
                                          task_manager::StartAction::kMainMenu);
                },
                bwi),
            kActionTaskManagerMainMenu, IDS_TASK_MANAGER, IDS_TASK_MANAGER,
            vector_icons::kTableChartIcon,
            /*is_pinnable=*/false)
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
                controller->ExecutePageAction(
                    CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi))
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

                chrome::BookmarkCurrentTab(bwi);
              },
              bwi))
          .SetActionId(kActionBookmarkThisTab)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_THIS_TAB)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_THIS_TAB)))
          .SetImage(ui::ImageModel::FromVectorIcon(omnibox::kStarIcon,
                                                   ui::kColorIcon))
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
            features::IsRoundedIconsEnabled() ? vector_icons::kScienceIcon
                                              : vector_icons::kScienceOldIcon)
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
                InspectUI::InspectDevices(bwi);
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
          .SetText(l10n_util::GetStringUTF16(IDS_PROFILING_ENABLED))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_PROFILING_ENABLED))
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
                BrowserSelectFileDialogController::From(bwi)->OpenFile();
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
          .SetText(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS_ITEM))
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_SUBMENU_MANAGE_EXTENSIONS_ITEM))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kChromeExtensionIcon
                  : vector_icons::kExtensionChromeRefreshOldIcon))
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
          .SetText(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_SUBMENU_CHROME_WEBSTORE_ITEM))
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_SUBMENU_CHROME_WEBSTORE_ITEM))
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          .SetImage(ui::ImageModel::FromVectorIcon(
              vector_icons::kGoogleChromeWebstoreIcon, ui::kColorIcon))
#endif
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
                const GURL& url = chrome::GetURLToBookmark(web_contents);
                IntentPickerTabHelper* const intent_picker_tab_helper =
                    IntentPickerTabHelper::From(
                        tab_strip_model->GetActiveTab());
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
          features::IsRoundedIconsEnabled() ? vector_icons::kLinkIcon
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
                    media_router::CastBrowserController::From(bwi);
                if (cast_browser_controller) {
                  cast_browser_controller->ToggleDialog();
                } else {
                  chrome::RouteMediaInvokedFromAppMenu(bwi);
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

  // TODO(crbug.com/435220196): Ideally this action would have
  // DownloadToolbarUIController passed in as a dependency directly.
  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
#if BUILDFLAG(IS_CHROMEOS)
                // ChromeOS does not use DownloadToolbarUIController (downloads
                // are managed via the Ash shelf/holding space), so directly
                // open the downloads WebUI page instead of showing the toolbar
                // bubble.
                chrome::ShowDownloads(webui::GetBrowserForOpeningWebUi(bwi));
#else
                if (auto* controller = DownloadToolbarUIController::From(bwi)) {
                  controller->InvokeUI();
                }
#endif
              },
              bwi),
          kActionShowDownloads, IDS_SHOW_DOWNLOADS, IDS_TOOLTIP_DOWNLOAD_ICON,
          features::IsRoundedIconsEnabled()
              ? kDownloadIcon
              : kDownloadToolbarButtonChromeRefreshOldIcon)
          .Build());

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
                    CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi))
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
                    CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi))
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
                    CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi))
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
                chrome::BrowserCommandController::From(bwi)
                    ->ShowCustomizeChromeSidePanel(
                        SidePanelOpenTrigger::kAppMenu,
                        CustomizeChromeSection::kToolbar);
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
                base::RecordAction(
                    base::UserMetricsAction("InstallWebAppFromMenu"));
                web_app::CreateWebAppFromCurrentWebContents(
                    bwi, web_app::WebAppInstallFlow::kInstallSite);
              },
              bwi))
          .SetActionId(kActionInstallPwa)
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kInstallDesktopIcon
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_WINDOW)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NEW_WINDOW)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kNewWindowIcon
                                                : kNewWindowOldIcon,
              ui::kColorIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_NEW_WINDOW))
          .Build());
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
                CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi)).Cut();
              },
              bwi))
          .SetActionId(actions::kActionCut)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_CUT)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_CUT)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kContentCutIcon
                                                : kCutMenuOldIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_CUT))
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi)).Copy();
              },
              bwi))
          .SetActionId(actions::kActionCopy)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_COPY)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_COPY)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? vector_icons::kContentCopyIcon
                                                : kCopyMenuOldIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_COPY))
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                CHECK_DEREF(BrowserView::GetBrowserViewForBrowser(bwi)).Paste();
              },
              bwi))
          .SetActionId(actions::kActionPaste)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_PASTE)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_PASTE)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kContentPasteIcon
                                                : kPasteMenuOldIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_PASTE))
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::BrowserCommandController::From(bwi)
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
                  chrome::UnfocusTabGroup(
                      bwi, TabGroupFocusExitReason::kTabStripButton);
                },
                bwi))
            .SetActionId(kActionUnfocusTabGroup)
            .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
                l10n_util::GetStringUTF16(
                    IDS_TAB_GROUP_HEADER_CXMENU_UNFOCUS_GROUP)))
            .SetImage(ui::ImageModel::FromVectorIcon(
                features::IsRoundedIconsEnabled()
                    ? vector_icons::kArrowBackIcon
                    : vector_icons::kArrowBackOldIcon,
                ui::kColorIcon))
            .SetVisible(false)
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
                [](BrowserWindowInterface* browser, actions::ActionId action_id,
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
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  PrefService* profile_prefs = bwi->GetProfile()->GetPrefs();
                  profile_prefs->SetBoolean(
                      glic::prefs::kGlicPinnedToTabstrip,
                      !profile_prefs->GetBoolean(
                          glic::prefs::kGlicPinnedToTabstrip));
                },
                bwi))
            .SetActionId(kActionGlicTogglePin)
            .SetText(l10n_util::GetStringUTF16(IDS_GLIC_PIN))
            .Build());
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  auto* service =
                      glic::GlicKeyedService::Get(bwi->GetProfile());
                  if (service) {
                    service->ShowUI(
                        bwi, glic::mojom::InvocationSource::kThreeDotsMenu);
                  }
                },
                bwi),
            kActionOpenGlic, IDS_GLIC_THREE_DOT_MENU_ITEM,
            IDS_GLIC_THREE_DOT_MENU_ITEM,
            glic::GlicVectorIconManager::GetVectorIcon(
                IDR_GLIC_BUTTON_VECTOR_ICON),
            /*is_pinnable=*/false)
            .Build());
  }

  if (base::FeatureList::IsEnabled(features::kIndigo)) {
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
                        case page_actions::PageActionEntryPoint::
                            kSuggestionChip:
                          return indigo::EntryPoint::kSuggestionChip;
                        case page_actions::PageActionEntryPoint::
                            kAnchoredMessage:
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
                glic::GlicVectorIconManager::GetVectorIcon(
                    IDR_GLIC_BUTTON_VECTOR_ICON),
                ui::kColorSysOnSurface))
            .SetText(l10n_util::GetStringUTF16(IDS_INDIGO_ENTRYPOINT_CHIP_TEXT))
            .Build());
  }

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
                vector_icons::kFastForwardCircleSparkIcon
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
                      tab->GetTabFeatures()->contextual_cueing_controller();
                  if (controller) {
                    controller->OnActionInvoked();
                  }
                },
                bwi))
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
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ExecuteVisitDesktopCommand(
                    IDC_VISIT_DESKTOP_OF_LRU_USER_2,
                    BrowserWindow::FromBrowser(bwi)->GetNativeWindow());
              },
              bwi))
          .SetActionId(kActionVisitDesktopOfLruUser2)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ExecuteVisitDesktopCommand(
                    IDC_VISIT_DESKTOP_OF_LRU_USER_3,
                    BrowserWindow::FromBrowser(bwi)->GetNativeWindow());
              },
              bwi))
          .SetActionId(kActionVisitDesktopOfLruUser3)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ExecuteVisitDesktopCommand(
                    IDC_VISIT_DESKTOP_OF_LRU_USER_4,
                    BrowserWindow::FromBrowser(bwi)->GetNativeWindow());
              },
              bwi))
          .SetActionId(kActionVisitDesktopOfLruUser4)
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ExecuteVisitDesktopCommand(
                    IDC_VISIT_DESKTOP_OF_LRU_USER_5,
                    BrowserWindow::FromBrowser(bwi)->GetNativeWindow());
              },
              bwi))
          .SetActionId(kActionVisitDesktopOfLruUser5)
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
                chrome::ToggleTabSearchPin(bwi);
              },
              bwi))
          .SetActionId(kActionTabSearchTogglePin)
          .SetText(l10n_util::GetStringUTF16(IDS_TAB_STRIP_PIN_TAB_SEARCH))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleTabScrollButtonsPin(bwi);
              },
              bwi))
          .SetActionId(kActionTabScrollTogglePin)
          .SetText(
              l10n_util::GetStringUTF16(IDS_TAB_SCROLL_PIN_BUTTONS_SYSTEM_MENU))
          .SetImage(
              ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon))
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
                chrome::ExecuteUIDebugCommand(IDC_DEBUG_PRINT_WINDOW_HIERARCHY,
                                              bwi);
              },
              bwi))
          .SetActionId(kActionDebugPrintWindowHierarchy)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ExecuteUIDebugCommand(IDC_DEBUG_PRINT_LAYER_HIERARCHY,
                                              bwi);
              },
              bwi))
          .SetActionId(kActionDebugPrintLayerHierarchy)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(base::UserMetricsAction("CloseWindowByKey"));
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
                chrome::ToggleFullscreenMode(bwi, /*user_initiated=*/true);
              },
              bwi))
          .SetActionId(kActionFullscreen)
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_FULLSCREEN))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kFullscreenIcon
                                                : kFullscreenRefreshOldIcon))
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_EXIT)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_EXIT)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kExitToAppIcon
                                                : kExitMenuOldIcon,
              ui::kColorIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_EXIT))
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

#if BUILDFLAG(IS_WIN)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::OpenMoveWindow(bwi);
              },
              bwi))
          .SetActionId(kActionMoveWindow)
          .SetText(l10n_util::GetStringUTF16(IDS_MOVE_WINDOW_MENU_WIN))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::OpenSizeWindow(bwi);
              },
              bwi))
          .SetActionId(kActionSizeWindow)
          .SetText(l10n_util::GetStringUTF16(IDS_SIZE_WINDOW_MENU_WIN))
          .Build());
#endif  // BUILDFLAG(IS_WIN)

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::PromptToNameWindow(bwi);
              },
              bwi))
          .SetActionId(kActionNameWindow)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NAME_WINDOW)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_NAME_WINDOW)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kWebAssetIcon
                                                : kNameWindowOldIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(
                    base::UserMetricsAction("OpenActiveTabInPwaWindow"));
                web_app::ReparentWebAppForActiveTab(bwi);
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
                chrome::ToggleAlwaysShowToolbarInFullscreen(bwi);
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
                base::RecordAction(base::UserMetricsAction("CloseTabByKey"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNextTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectPreviousTab"));
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
                if (chrome::IsCtrlTabMruEnabled(bwi)) {
                  base::RecordAction(
                      base::UserMetricsAction("Accel_CycleToNextTab"));
                  chrome::CycleToMruTab(bwi);
                } else {
                  base::RecordAction(
                      base::UserMetricsAction("Accel_SelectNextTab"));
                  chrome::SelectNextTab(bwi);
                }
              },
              bwi))
          .SetActionId(kActionCycleToNextTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (chrome::IsCtrlTabMruEnabled(bwi)) {
                  base::RecordAction(
                      base::UserMetricsAction("Accel_CycleToPrevTab"));
                  chrome::CycleToMruTab(bwi);
                } else {
                  base::RecordAction(
                      base::UserMetricsAction("Accel_SelectPreviousTab"));
                  chrome::SelectPreviousTab(bwi);
                }
              },
              bwi))
          .SetActionId(kActionCycleToPrevTab)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_SelectNumberedTab"));
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_ALL_TABS)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_ALL_TABS)))
          .SetImage(
              ui::ImageModel::FromVectorIcon(kHotelClassIcon, ui::kColorIcon))
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
                base::UmaHistogramEnumeration(
                    "TabGroups.Shortcuts",
                    chrome::TabGroupShortcut::kAddNewTabToGroup);
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
                base::UmaHistogramEnumeration(
                    "TabGroups.Shortcuts",
                    chrome::TabGroupShortcut::kCreateNewTabGroup);
              },
              bwi))
          .SetActionId(kActionCreateNewTabGroup)
          .SetText(l10n_util::GetStringUTF16(IDS_CREATE_NEW_TAB_GROUP))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kLibraryAddIcon
                                                : kCreateNewTabGroupOldIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (base::i18n::IsRTL()) {
                  chrome::FocusPreviousTabGroup(bwi);
                } else {
                  chrome::FocusNextTabGroup(bwi);
                }
                base::UmaHistogramEnumeration(
                    "TabGroups.Shortcuts",
                    chrome::TabGroupShortcut::kFocusNextTabGroup);
              },
              bwi))
          .SetActionId(kActionFocusNextTabGroup)
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (base::i18n::IsRTL()) {
                  chrome::FocusNextTabGroup(bwi);
                } else {
                  chrome::FocusPreviousTabGroup(bwi);
                }
                base::UmaHistogramEnumeration(
                    "TabGroups.Shortcuts",
                    chrome::TabGroupShortcut::kFocusPrevTabGroup);
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
                base::UmaHistogramEnumeration(
                    "TabGroups.Shortcuts",
                    chrome::TabGroupShortcut::kCloseTabGroup);
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
                base::RecordAction(
                    base::UserMetricsAction("TabGroups_GroupAllUngroupedTabs"));
              },
              bwi))
          .SetActionId(kActionGroupUngroupedTabs)
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
          .SetText(l10n_util::GetStringUTF16(IDS_READING_LIST_MENU_ADD_TAB))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kListAltAddIcon
                                                : kReadLaterAddOldIcon,
              ui::kColorIcon))
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_Focus_Toolbar"));
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
                if (!BrowserWindow::FromBrowser(bwi)->IsLocationBarVisible()) {
                  return;
                }
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_Focus_Search"));
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
                base::RecordAction(
                    base::UserMetricsAction("Accel_Focus_Bookmarks"));
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
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM_PLUS2))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kAddIcon
                                                : kZoomPlusMenuRefreshOldIcon))
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
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM_MINUS2))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kRemoveIcon
                                                : kZoomMinusMenuRefreshOldIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::Zoom(bwi, content::PAGE_ZOOM_RESET);
              },
              bwi))
          .SetActionId(kActionZoomNormal)
          .SetText(l10n_util::GetStringUTF16(IDS_ZOOM_NORMAL))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_ZOOM))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kZoomInIcon : kZoomInOldIcon))
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
#if !BUILDFLAG(IS_ANDROID)
                  DevToolsPolicyDialog::Show(web_contents);
#endif  // !BUILDFLAG(IS_ANDROID)
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SAVE_PAGE)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SAVE_PAGE)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kFileSaveIcon
                  : kFileSaveChromeRefreshOldIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_SAVE_PAGE))
          .Build());

#if BUILDFLAG(ENABLE_PRINTING)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(
                    base::UserMetricsAction("Accel_Advanced_Print"));
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
                base::RecordAction(base::UserMetricsAction("CreateShortcut"));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
                chrome::CreateDesktopShortcutForActiveWebContents(bwi);
#else
                web_app::CreateWebAppFromCurrentWebContents(
                    bwi, web_app::WebAppInstallFlow::kCreateShortcut);
#endif
              },
              bwi))
          .SetActionId(kActionCreateShortcut)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_ADD_TO_OS_LAUNCH_SURFACE)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_ADD_TO_OS_LAUNCH_SURFACE)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? kDriveShortcutIcon
                  : kDriveShortcutChromeRefreshOldIcon))
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

#if !BUILDFLAG(IS_CHROMEOS)
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                ShowSyncPassphraseDialogAndDecryptData(*bwi);
              },
              bwi))
          .SetActionId(kActionShowSyncPassphraseDialog)
          .Build());
#endif  // !BUILDFLAG(IS_CHROMEOS)

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowPasswordCheck(bwi);
              },
              bwi))
          .SetActionId(kActionSafetyHubShowPasswordCheckup)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowContactInfo(bwi);
              },
              bwi),
          kActionShowContactInfo,
          IDS_YOUR_SAVED_INFO_CONTACT_INFO_SUBMENU_OPTION,
          IDS_YOUR_SAVED_INFO_CONTACT_INFO_SUBMENU_OPTION,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kLocationOnIcon
              : vector_icons::kLocationOnChromeRefreshOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowIdentityDocs(bwi);
              },
              bwi),
          kActionShowIdentityDocs, IDS_IDENTITY_DOCS_SUBMENU_OPTION,
          IDS_IDENTITY_DOCS_SUBMENU_OPTION,
          features::IsRoundedIconsEnabled() ? vector_icons::kIdCardIcon
                                            : vector_icons::kIdCardOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowTravel(bwi);
              },
              bwi),
          kActionShowTravel, IDS_TRAVEL_SUBMENU_OPTION,
          IDS_TRAVEL_SUBMENU_OPTION,
          features::IsRoundedIconsEnabled() ? vector_icons::kTripIcon
                                            : vector_icons::kTripOldIcon,
          /*is_pinnable=*/false)
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
                chrome::ShowSettings(bwi);
              },
              bwi))
          .SetActionId(kActionOptions)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SETTINGS)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SETTINGS)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kSettingsIcon
                                                : kSettingsMenuOldIcon,
              ui::kColorIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_OPTIONS))
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
          .SetText(l10n_util::GetStringUTF16(IDS_IMPORT_SETTINGS_MENU_LABEL))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kMenuBookIcon
                                                : kMenuBookChromeRefreshOldIcon,
              ui::kColorIcon))
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
                chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(
                    bwi->GetProfile());
                DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
                    DefaultBrowserPromptManager::CloseReason::kAccept);
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

  const gfx::VectorIcon& lens_icon =
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
                chrome::ExecLensOverlay(bwi);
              },
              bwi))
          .SetActionId(kActionShowLensOverlayFromAppMenu)
          .SetText(l10n_util::GetStringUTF16(
              lens::GetLensOverlayEntrypointLabelAltIds()))
          .SetTooltipText(l10n_util::GetStringUTF16(
              lens::GetLensOverlayEntrypointLabelAltIds()))
          .SetImage(ui::ImageModel::FromVectorIcon(lens_icon, ui::kColorIcon))
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

#if !BUILDFLAG(IS_CHROMEOS)
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
#endif

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ToggleBookmarkBar(bwi);
              },
              bwi))
          .SetActionId(kActionShowBookmarkBar)
          .SetText(l10n_util::GetStringUTF16(IDS_SHOW_BOOKMARK_BAR))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kToolbarIcon
                                                : kToolbarChromeRefreshOldIcon,
              ui::kColorIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(base::UserMetricsAction(
                    "WrenchMenu_Bookmarks_AlwaysShowBookmarkBar"));
                chrome::SetBookmarkBarVisibilityState(
                    bwi, bookmarks::BookmarkBarVisibilityState::kAlwaysShow);
              },
              bwi))
          .SetActionId(kActionBookmarkBarSubmenuAlwaysShow)
          .SetText(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_SUBMENU_ALWAYS_SHOW))
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(base::UserMetricsAction(
                    "WrenchMenu_Bookmarks_AlwaysHideBookmarkBar"));
                chrome::SetBookmarkBarVisibilityState(
                    bwi, bookmarks::BookmarkBarVisibilityState::kAlwaysHide);
              },
              bwi))
          .SetActionId(kActionBookmarkBarSubmenuAlwaysHide)
          .SetText(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_SUBMENU_ALWAYS_HIDE))
          .Build());
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(base::UserMetricsAction(
                    "WrenchMenu_Bookmarks_OnlyShowBookmarkBarOnNtp"));
                chrome::SetBookmarkBarVisibilityState(
                    bwi, bookmarks::BookmarkBarVisibilityState::kOnlyShowOnNtp);
              },
              bwi))
          .SetActionId(kActionBookmarkBarSubmenuOnlyOnNtp)
          .SetText(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_SUBMENU_ONLY_ON_NTP))
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
          .SetText(l10n_util::GetStringUTF16(
              features::IsMenuSimplificationEnabled() ? IDS_BOOKMARK_MANAGER_V2
                                                      : IDS_BOOKMARK_MANAGER))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kBookmarkManagerIcon
                                                : kBookmarksManagerOldIcon,
              ui::kColorIcon))
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_ABOUT)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_ABOUT)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kInfoIcon
                  : vector_icons::kInfoRefreshOldIcon))
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
                auto* controller = web_app::AppBrowserController::From(bwi);
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SHARING_HUB_SCREENSHOT_LABEL)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SHARING_HUB_SCREENSHOT_LABEL)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kScreenshotRegionIcon
                                                : kSharingHubScreenshotOldIcon))
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_FEEDBACK)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_FEEDBACK)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kFeedbackIcon
                                                : kReportOldIcon))
          .Build());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* browser_window_interface,
                 actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::OpenReportUnsafeSiteDialog(browser_window_interface);
              },
              bwi),
          kActionReportUnsafeSite, IDS_REPORT_UNSAFE_SITE,
          IDS_REPORT_UNSAFE_SITE,
          features::IsRoundedIconsEnabled() ? vector_icons::kWarningFilledIcon
                                            : vector_icons::kWarningOldIcon,
          /*is_pinnable=*/false)
          .Build());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowHistory(bwi);
              },
              bwi))
          .SetActionId(kActionShowHistory)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_HISTORY_MENU)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_HISTORY_MENU)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kHistoryIcon
                  : vector_icons::kHistoryChromeRefreshOldIcon,
              ui::kColorIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_SHOW_HISTORY))
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

#if BUILDFLAG(IS_CHROMEOS) && defined(OFFICIAL_BUILD)
  int help_string_id = IDS_GET_HELP;
#else
  int help_string_id = IDS_HELP_PAGE;
#endif
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowHelp(bwi, chrome::HelpSource::kMenu);
              },
              bwi))
          .SetActionId(kActionHelpPageViaMenu)
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(help_string_id)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(help_string_id)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kHelpCustomIcon
                                                : kHelpMenuOldIcon))
          .Build());

  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                base::RecordAction(
                    base::UserMetricsAction("Accel_Show_App_Menu"));
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled()
                  ? vector_icons::kChromeExtensionIcon
                  : vector_icons::kExtensionChromeRefreshOldIcon,
              ui::kColorIcon))
          .SetAccelerator(GetAcceleratorForCommandId(IDC_MANAGE_EXTENSIONS))
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowExtensions(bwi);
              },
              bwi),
          kActionSafetyHubManageExtensions, IDS_MANAGE_EXTENSIONS,
          IDS_MANAGE_EXTENSIONS,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kChromeExtensionIcon
              : vector_icons::kExtensionChromeRefreshOldIcon,
          /*is_pinnable=*/false)
          .Build());
  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowWebStore(bwi, extension_urls::kAppMenuUtmSource);
              },
              bwi),
          kActionFindExtensions, IDS_FIND_EXTENSIONS, IDS_FIND_EXTENSIONS,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kChromeExtensionIcon
              : vector_icons::kExtensionChromeRefreshOldIcon,
          /*is_pinnable=*/false)
          .Build());
  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                chrome::ShowSettingsSubPage(bwi, chrome::kSafetyHubSubPage);
              },
              bwi),
          kActionOpenSafetyHub, IDS_SETTINGS_SAFETY_HUB,
          IDS_SETTINGS_SAFETY_HUB,
          features::IsRoundedIconsEnabled() ? kSecurityIcon : kSecurityOldIcon,
          /*is_pinnable=*/false)
          .Build());
  if (base::FeatureList::IsEnabled(features::kEnterpriseReleaseNotes)) {
    root_action_item_->AddChild(
        ChromeMenuAction(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  chrome::ShowChromeEnterpriseReleaseNotes(bwi);
                },
                bwi),
            kActionChromeEnterpriseReleaseNotes,
            IDS_CHROME_ENTERPRISE_RELEASE_NOTES,
            IDS_CHROME_ENTERPRISE_RELEASE_NOTES, omnibox::kChromeProductIcon,
            /*is_pinnable=*/false)
            .Build());
  }

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
                  chrome::ToggleJavaScriptFromAppleEventsAllowed(bwi);
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_CHROME_WHATS_NEW)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_CHROME_WHATS_NEW)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kReleaseAlertIcon
                                                : kReleaseAlertOldIcon))
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
          .SetText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SHOW_PERFORMANCE)))
          .SetTooltipText(BrowserActions::GetCleanTitleAndTooltipText(
              l10n_util::GetStringUTF16(IDS_SHOW_PERFORMANCE)))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kSpeedIcon
                                                : kPerformanceOldIcon))
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
  root_action_item_->AddChild(
      actions::ActionItem::Builder(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {
                if (!base::FeatureList::IsEnabled(
                        autofill::features::
                            kAutofillEnableResurrectingPaymentsUsers)) {
                  return;
                }

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

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableWalletReminderNotice) ||
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableWalletReminderNoticePublicPass)) {
    root_action_item_->AddChild(
        actions::ActionItem::Builder(
            base::BindRepeating(
                [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                   actions::ActionInvocationContext context) {
                  tabs::TabInterface* tab = bwi->GetActiveTabInterface();
                  CHECK(tab);

                  if (auto* controller =
                          autofill::WalletReminderNoticeBubbleController::From(
                              *tab)) {
                    controller->ReshowBubble();
                  }
                },
                bwi))
            .SetActionId(kActionWalletReminderNotice)
            .SetImage(ui::ImageModel::FromVectorIcon(kWalletIcon))
            .Build());
  }
#endif  // !BUILDFLAG(IS_ANDROID)
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

void BrowserActions::InitializeSubmenuActions() {
  BrowserWindowInterface* const bwi = base::to_address(bwi_);

  root_action_item_->AddChild(
      actions::ActionItem::Builder().SetActionId(kActionAppMenuRoot).Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionBookmarksSubmenu, IDS_BOOKMARKS_AND_LISTS_MENU,
          IDS_BOOKMARKS_AND_LISTS_MENU,
          features::IsRoundedIconsEnabled() ? kStarIcon
                                            : kBookmarksListsMenuOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionBookmarkBarSubmenu, IDS_BOOKMARK_BAR_SUBMENU_LABEL,
          IDS_BOOKMARK_BAR_SUBMENU_LABEL,
          features::IsRoundedIconsEnabled() ? kToolbarIcon
                                            : kToolbarChromeRefreshOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionPasswordsAndAutofillSubmenu, IDS_PASSWORDS_AND_AUTOFILL_MENU,
          IDS_PASSWORDS_AND_AUTOFILL_MENU,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kPasswordManagerIcon
              : vector_icons::kPasswordManagerOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionReadingListSubmenu, IDS_READING_LIST_MENU,
          IDS_READING_LIST_MENU,
          features::IsRoundedIconsEnabled() ? kListAltIcon
                                            : kReadingListOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionZoomSubmenu, IDS_ZOOM_MENU, IDS_ZOOM_MENU,
          features::IsRoundedIconsEnabled() ? kZoomInIcon : kZoomInOldIcon,
          /*is_pinnable=*/false)
          .Build());

  // TODO(crbug.com/538215007): Need to ensure that profile submenu is correct
  const gfx::VectorIcon& avatar_vector_icon =
      profile_->IsIncognitoProfile()
          ? (features::IsRoundedIconsEnabled() ? kIncognitoCircleFilledIcon
                                               : kIncognitoOldIcon)
          : (features::IsRoundedIconsEnabled()
                 ? kAccountCircleIcon
                 : kAccountCircleChromeRefreshOldIcon);

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionProfileSubmenu, IDS_READING_LIST_MENU, IDS_READING_LIST_MENU,
          avatar_vector_icon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionFindAndEditSubmenu, IDS_FIND_AND_EDIT_MENU,
          IDS_FIND_AND_EDIT_MENU,
          features::IsRoundedIconsEnabled() ? kFindInPageIcon
                                            : kSearchMenuOldIcon,
          /*is_pinnable=*/false)
          .Build());

  int save_and_share_menu_string_id =
      media_router::MediaRouterEnabled(bwi->GetProfile())
          ? IDS_CAST_SAVE_AND_SHARE_MENU
          : IDS_SAVE_AND_SHARE_MENU;

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionSaveAndShareSubmenu, save_and_share_menu_string_id,
          save_and_share_menu_string_id,
          features::IsRoundedIconsEnabled() ? kFileSaveIcon
                                            : kFileSaveChromeRefreshOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionHelpSubmenu, IDS_HELP_MENU, IDS_HELP_MENU,
          features::IsRoundedIconsEnabled() ? kHelpCustomIcon
                                            : kHelpMenuOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionSavedTabGroupsSubmenu, IDS_SAVED_TAB_GROUPS_MENU,
          IDS_SAVED_TAB_GROUPS_MENU,
          features::IsRoundedIconsEnabled()
              ? kGridViewIcon
              : kSavedTabGroupBarEverythingOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionRecentTabsSubmenu, IDS_HISTORY_MENU, IDS_HISTORY_MENU,
          features::IsRoundedIconsEnabled() ? kHistoryIcon : kHistoryOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionDeveloperSubmenu, IDS_MORE_TOOLS_MENU, IDS_MORE_TOOLS_MENU,
          features::IsRoundedIconsEnabled() ? kHomeRepairServiceIcon
                                            : kMoreToolsMenuOldIcon,
          /*is_pinnable=*/false)
          .Build());

  root_action_item_->AddChild(
      ChromeMenuAction(
          base::BindRepeating(
              [](BrowserWindowInterface* bwi, actions::ActionItem* item,
                 actions::ActionInvocationContext context) {},
              bwi),
          kActionExtensionsSubmenu, IDS_EXTENSIONS_SUBMENU,
          IDS_EXTENSIONS_SUBMENU,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kChromeExtensionIcon
              : vector_icons::kExtensionChromeRefreshOldIcon,
          /*is_pinnable=*/false)
          .Build());
}

void BrowserActions::AddListeners() {
  browser_action_prefs_listener_ = std::make_unique<BrowserActionPrefsListener>(
      base::to_address(profile_), this);
}
