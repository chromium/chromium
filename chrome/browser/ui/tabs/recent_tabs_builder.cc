// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/recent_tabs_builder.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/history_clusters/history_clusters_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/favicon_size.h"

namespace {

constexpr int kMaxLocalEntries = 8;
constexpr size_t kMaxSessionsToShow = 8;

bool SortSessionsByRecency(const sync_sessions::SyncedSession* s1,
                           const sync_sessions::SyncedSession* s2) {
  return s1->GetModifiedTime() > s2->GetModifiedTime();
}

ui::ImageModel CreateFavicon(const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorMenuIcon,
                                        gfx::kFaviconSize);
}

std::u16string GetGroupItemLabel(const std::u16string& title, size_t num_tabs) {
  if (title.empty()) {
    return l10n_util::GetPluralStringFUTF16(IDS_RECENTLY_CLOSED_GROUP_UNNAMED,
                                            static_cast<int>(num_tabs));
  }
  std::u16string item_label = l10n_util::GetPluralStringFUTF16(
      IDS_RECENTLY_CLOSED_GROUP, static_cast<int>(num_tabs));
  return base::ReplaceStringPlaceholders(item_label, title, nullptr);
}

const gfx::VectorIcon& GetDeviceIcon(
    syncer::DeviceInfo::FormFactor device_form_factor) {
  switch (device_form_factor) {
    case syncer::DeviceInfo::FormFactor::kPhone:
      return features::IsRoundedIconsEnabled() ? kMobileIcon
                                               : kSmartphoneOldIcon;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return features::IsRoundedIconsEnabled() ? kTabletFilledIcon
                                               : kTabletOldIcon;
    default:
      return features::IsRoundedIconsEnabled() ? kLaptopWindowsIcon
                                               : kLaptopOldIcon;
  }
}

RecentTabItem BuildTabItem(const sessions::tab_restore::Tab& tab) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.normalized_navigation_index());
  std::u16string title =
      current_navigation.title().empty()
          ? base::UTF8ToUTF16(current_navigation.virtual_url().spec())
          : current_navigation.title();

  RecentTabItem item(RecentTabItem::Type::kTab, title);
  item.set_session_id(tab.id);
  item.set_url(current_navigation.virtual_url());
  item.set_icon(favicon::GetDefaultFaviconModel());
  item.set_is_local(true);

  if (tab.group_visual_data.has_value()) {
    const ui::ColorId color_id =
        GetTabGroupContextMenuColorId(tab.group_visual_data.value().color());
    constexpr int kIconSize = 12;
    item.set_minor_icon(ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? kCircleFilledIcon
                                          : kTabGroupOldIcon,
        color_id, kIconSize));
  }

  return item;
}

RecentTabItem BuildWindowItem(const sessions::tab_restore::Window& window) {
  std::u16string label = l10n_util::GetPluralStringFUTF16(
      IDS_RECENTLY_CLOSED_WINDOW, static_cast<int>(window.tabs.size()));

  RecentTabItem window_item(RecentTabItem::Type::kWindow, label);
  window_item.set_session_id(window.id);
  window_item.set_icon(CreateFavicon(
      features::IsRoundedIconsEnabled() ? kTabIcon : kTabOldIcon));

  RecentTabItem restore_cmd(RecentTabItem::Type::kCommand,
                            l10n_util::GetStringUTF16(IDS_RESTORE_WINDOW));
  restore_cmd.set_session_id(window.id);
  restore_cmd.set_icon(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kOpenInNewFlippableIcon
                                        : vector_icons::kLaunchOldIcon,
      ui::kColorMenuIcon, gfx::kFaviconSize));
  window_item.add_child(std::move(restore_cmd));

  for (const auto& tab : window.tabs) {
    window_item.add_child(BuildTabItem(*tab));
  }

  return window_item;
}

RecentTabItem BuildGroupItem(const sessions::tab_restore::Group& group) {
  std::u16string item_label =
      GetGroupItemLabel(group.visual_data.title(), group.tabs.size());
  const ui::ColorId color_id =
      GetTabGroupContextMenuColorId(group.visual_data.color());
  ui::ImageModel group_icon = ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kCircleFilledIcon : kTabGroupOldIcon,
      color_id, gfx::kFaviconSize);

  RecentTabItem group_item(RecentTabItem::Type::kGroup, item_label);
  group_item.set_session_id(group.id);
  group_item.set_icon(group_icon);

  RecentTabItem restore_cmd(RecentTabItem::Type::kCommand,
                            l10n_util::GetStringUTF16(IDS_RESTORE_GROUP));
  restore_cmd.set_session_id(group.id);
  restore_cmd.set_icon(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? vector_icons::kOpenInNewFlippableIcon
                                        : vector_icons::kLaunchOldIcon,
      ui::kColorMenuIcon, gfx::kFaviconSize));
  group_item.add_child(std::move(restore_cmd));

  for (const auto& tab : group.tabs) {
    group_item.add_child(BuildTabItem(*tab));
  }

  return group_item;
}

RecentTabItem BuildSplitItem(const sessions::tab_restore::Split& split) {
  std::u16string item_label =
      l10n_util::GetStringUTF16(IDS_RECENTLY_CLOSED_SPLIT);

  const gfx::VectorIcon* icon = nullptr;
  if (split.visual_data.split_layout() ==
      split_tabs::SplitTabLayout::kStacked) {
    icon = &kSplitScene2Icon;
  } else {
    icon = &(features::IsRoundedIconsEnabled() ? kSplitSceneIcon
                                               : kSplitSceneOldIcon);
  }

  RecentTabItem split_item(RecentTabItem::Type::kSplit, item_label);
  split_item.set_session_id(split.id);
  split_item.set_icon(CreateFavicon(*icon));

  RecentTabItem restore_cmd(RecentTabItem::Type::kCommand,
                            l10n_util::GetStringUTF16(IDS_RESTORE_SPLIT));
  restore_cmd.set_session_id(split.id);
  restore_cmd.set_icon(ui::ImageModel::FromVectorIcon(
      vector_icons::kLaunchOldIcon, ui::kColorMenuIcon, gfx::kFaviconSize));
  split_item.add_child(std::move(restore_cmd));

  for (const auto& tab : split.tabs) {
    split_item.add_child(BuildTabItem(*tab));
  }

  return split_item;
}

}  // namespace

RecentTabItem::RecentTabItem(Type type, std::u16string title)
    : type_(type), title_(std::move(title)) {}
RecentTabItem::RecentTabItem(const RecentTabItem&) = default;
RecentTabItem::RecentTabItem(RecentTabItem&&) = default;
RecentTabItem& RecentTabItem::operator=(const RecentTabItem&) = default;
RecentTabItem& RecentTabItem::operator=(RecentTabItem&&) = default;
RecentTabItem::~RecentTabItem() = default;

std::vector<RecentTabItem> RecentTabsBuilder::BuildRecentTabs(
    Profile* profile,
    BrowserWindowInterface* browser) {
  std::vector<RecentTabItem> items;
  if (!profile || !browser) {
    return items;
  }

  auto history_items = BuildHistoryEntries(profile, browser);
  items.insert(items.end(), std::make_move_iterator(history_items.begin()),
               std::make_move_iterator(history_items.end()));

  auto local_items = BuildLocalEntries(profile);
  items.insert(items.end(), std::make_move_iterator(local_items.begin()),
               std::make_move_iterator(local_items.end()));

  auto remote_items = BuildRemoteEntries(profile);
  items.insert(items.end(), std::make_move_iterator(remote_items.begin()),
               std::make_move_iterator(remote_items.end()));

  return items;
}

std::vector<RecentTabItem> RecentTabsBuilder::BuildHistoryEntries(
    Profile* profile,
    BrowserWindowInterface* browser) {
  std::vector<RecentTabItem> items;
  if (!profile || !browser) {
    return items;
  }

  RecentTabItem history(RecentTabItem::Type::kCommand,
                        l10n_util::GetStringUTF16(IDS_HISTORY_SHOW_HISTORY));
  history.set_action_id(kActionShowHistory);
  history.set_icon(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled()
          ? vector_icons::kHistoryIcon
          : vector_icons::kHistoryChromeRefreshOldIcon,
      ui::kColorMenuIcon, gfx::kFaviconSize));
  items.push_back(std::move(history));

  if (SidePanelUI::From(browser)) {
    if (HistoryClustersSidePanelCoordinator::IsSupported(profile)) {
      RecentTabItem clusters(
          RecentTabItem::Type::kCommand,
          l10n_util::GetStringUTF16(IDS_HISTORY_CLUSTERS_SHOW_SIDE_PANEL));
      clusters.set_action_id(kActionSidePanelShowHistoryCluster);
      clusters.set_icon(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled()
              ? vector_icons::kHistoryIcon
              : vector_icons::kHistoryChromeRefreshOldIcon,
          ui::kColorMenuIcon, gfx::kFaviconSize));
      items.push_back(std::move(clusters));
    }

    if (TabsFromOtherDevicesSidePanelCoordinator::IsSupported(profile)) {
      RecentTabItem other_devices(
          RecentTabItem::Type::kCommand,
          l10n_util::GetStringUTF16(
              IDS_SIDE_PANEL_SHOW_TABS_FROM_OTHER_DEVICES));
      other_devices.set_action_id(kActionSidePanelShowTabsFromOtherDevices);
      other_devices.set_icon(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kDevicesIcon
                                            : kDevicesChromeRefreshOldIcon,
          ui::kColorMenuIcon, gfx::kFaviconSize));
      items.push_back(std::move(other_devices));
    }
  }

  return items;
}

std::vector<RecentTabItem> RecentTabsBuilder::BuildLocalEntries(
    Profile* profile) {
  std::vector<RecentTabItem> items;
  if (!profile) {
    return items;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);

  items.emplace_back(RecentTabItem::Type::kHeader,
                     l10n_util::GetStringUTF16(IDS_RECENT_TABS));

  int added_count = 0;
  for (const auto& entry : service->entries()) {
    if (added_count == kMaxLocalEntries) {
      break;
    }
    switch (entry->type) {
      case sessions::tab_restore::Type::TAB: {
        const auto& tab =
            static_cast<const sessions::tab_restore::Tab&>(*entry);
        items.push_back(BuildTabItem(tab));
        break;
      }
      case sessions::tab_restore::Type::WINDOW: {
        const auto& window =
            static_cast<const sessions::tab_restore::Window&>(*entry);
        items.push_back(BuildWindowItem(window));
        break;
      }
      case sessions::tab_restore::Type::GROUP: {
        const auto& group =
            static_cast<const sessions::tab_restore::Group&>(*entry);
        items.push_back(BuildGroupItem(group));
        break;
      }
      case sessions::tab_restore::Type::SPLIT: {
        const auto& split =
            static_cast<const sessions::tab_restore::Split&>(*entry);
        items.push_back(BuildSplitItem(split));
        break;
      }
    }
    ++added_count;
  }

  return items;
}

std::vector<RecentTabItem> RecentTabsBuilder::BuildRemoteEntries(
    Profile* profile) {
  std::vector<RecentTabItem> items;
  if (!profile) {
    return items;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile);
    if (!sync_service ||
        !signin_util::IsSyncingUserSelectableTypesAllowedByPolicy(
            sync_service, {syncer::UserSelectableType::kTabs})) {
      return items;
    }
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    switch (signin_util::GetSignedInState(identity_manager)) {
      case signin_util::SignedInState::kSignedIn:
      case signin_util::SignedInState::kSignInPending:
        if (signin_util::HasExplicitlyDisabledHistorySync(sync_service,
                                                          identity_manager)) {
          return items;
        }
        break;
      case signin_util::SignedInState::kSignedOut:
      case signin_util::SignedInState::kWebOnlySignedIn:
      case signin_util::SignedInState::kSyncing:
      case signin_util::SignedInState::kSyncPaused:
        break;
    }
  }
#endif

  items.emplace_back(RecentTabItem::Type::kHeader,
                     l10n_util::GetStringUTF16(IDS_YOUR_DEVICES));

  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service ? session_sync_service->GetOpenTabsUIDelegate()
                           : nullptr;

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  if (!open_tabs || !open_tabs->GetAllForeignSessions(&sessions)) {
    if (open_tabs) {
      RecentTabItem no_tabs(
          RecentTabItem::Type::kCommand,
          l10n_util::GetStringUTF16(IDS_RECENT_TABS_NO_DEVICE_TABS));
      no_tabs.set_enabled(false);
      items.push_back(std::move(no_tabs));
    } else if (syncer::IsReplaceSyncPromosWithSignInPromosEnabled()) {
      RecentTabItem see_tabs(
          RecentTabItem::Type::kCommand,
          l10n_util::GetStringUTF16(IDS_RECENT_TABS_SEE_DEVICE_TABS));
      see_tabs.set_action_id(kActionRecentTabsSeeDeviceTabs);
      see_tabs.set_icon(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? vector_icons::kSyncIcon
                                            : kSyncRefreshOldIcon,
          ui::kColorMenuIcon, gfx::kFaviconSize));
      items.push_back(std::move(see_tabs));
    } else {
      RecentTabItem login_tabs(
          RecentTabItem::Type::kCommand,
          l10n_util::GetStringUTF16(IDS_RECENT_TABS_LOGIN_FOR_DEVICE_TABS));
      login_tabs.set_action_id(kActionRecentTabsLoginForDeviceTabs);
      login_tabs.set_icon(ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? vector_icons::kSyncIcon
                                            : kSyncRefreshOldIcon,
          ui::kColorMenuIcon, gfx::kFaviconSize));
      items.push_back(std::move(login_tabs));
    }
    return items;
  }

  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);

  size_t num_sessions_added = 0;
  for (size_t i = 0;
       i < sessions.size() && num_sessions_added < kMaxSessionsToShow; ++i) {
    const sync_sessions::SyncedSession* session = sessions[i];
    const std::string& session_tag = session->GetSessionTag();

    std::vector<const sessions::SessionTab*> tabs_in_session;
    if (!open_tabs->GetForeignSessionTabs(session_tag, &tabs_in_session) ||
        tabs_in_session.empty()) {
      continue;
    }

    RecentTabItem device_item(RecentTabItem::Type::kDevice,
                              base::UTF8ToUTF16(session->GetSessionName()));
    device_item.set_session_tag(session_tag);
    device_item.set_device_form_factor(session->GetDeviceFormFactor());
    device_item.set_icon(
        CreateFavicon(GetDeviceIcon(session->GetDeviceFormFactor())));

    for (const auto* tab : tabs_in_session) {
      const sessions::SerializedNavigationEntry& current_navigation =
          tab->navigations.at(tab->normalized_navigation_index());
      std::u16string title =
          current_navigation.title().empty()
              ? base::UTF8ToUTF16(current_navigation.virtual_url().spec())
              : current_navigation.title();

      RecentTabItem foreign_tab(RecentTabItem::Type::kTab, title);
      foreign_tab.set_session_tag(session_tag);
      foreign_tab.set_session_id(tab->tab_id);
      foreign_tab.set_url(current_navigation.virtual_url());
      foreign_tab.set_icon(favicon::GetDefaultFaviconModel());
      foreign_tab.set_is_local(false);

      device_item.add_child(std::move(foreign_tab));
    }

    items.push_back(std::move(device_item));
    ++num_sessions_added;
  }

  return items;
}
