// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// The maximum number of local recently closed entries (tab or window) to be
// shown in the menu.
const int kMaxLocalEntries = 8;

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const sync_sessions::SyncedSession* s1,
                           const sync_sessions::SyncedSession* s2) {
  return s1->GetModifiedTime() > s2->GetModifiedTime();
}

ui::ImageModel CreateFavicon(const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorMenuIcon,
                                        gfx::kFaviconSize);
}

}  // namespace

enum RecentTabAction {
  LOCAL_SESSION_TAB = 0,
  OTHER_DEVICE_TAB,
  RESTORE_WINDOW,
  SHOW_MORE,
  LIMIT_RECENT_TAB_ACTION,
  RESTORE_GROUP
};

// An element in |RecentTabsSubMenuModel::local_tab_navigation_items_| or
// |RecentTabsSubMenuModel::other_devices_tab_navigation_items_| that stores
// the navigation information of a local or other devices' tab required to
// restore the tab.
struct RecentTabsSubMenuModel::TabNavigationItem {
  TabNavigationItem() : tab_id(SessionID::InvalidValue()) {}

  TabNavigationItem(const std::string& session_tag,
                    SessionID tab_id,
                    const std::u16string& title,
                    const GURL& url)
      : session_tag(session_tag), tab_id(tab_id), title(title), url(url) {}

  // For use by std::set for sorting.
  bool operator<(const TabNavigationItem& other) const {
    return url < other.url;
  }

  // Empty for local tabs, non-empty for other devices' tabs.
  std::string session_tag;
  SessionID tab_id;  // Might be invalid.
  std::u16string title;
  GURL url;
};

// An element in |RecentTabsSubMenuModel::sub_menu_items_| that records a sub
// menu item's model and its own command id.
// TODO(emshack): This solution, where sub menus are represented by a
// SimpleMenuModel and managed by the parent RecentTabsSubMenuModel, is not
// ideal. However, AppMenuModel requires unique ids across its descendant sub
// menus, and queries a single model for these ids. This doesn't work well with
// our preferred approach, which would decouple sub menus from their parent and
// allow them to manage their own items and command ids.
struct RecentTabsSubMenuModel::SubMenuItem {
  SubMenuItem(int command_id,
              std::unique_ptr<ui::SimpleMenuModel> sub_menu_model)
      : parent_id(command_id), menu_model(std::move(sub_menu_model)) {
    const size_t child_id_count = menu_model->GetItemCount();
    for (size_t i = 0; i < child_id_count; ++i) {
      child_ids.insert(menu_model->GetCommandIdAt(i));
    }
  }

  const int parent_id;
  std::unordered_set<int> child_ids;
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
};

RecentTabsSubMenuModel::RecentTabsSubMenuModel(
    ui::AcceleratorProvider* accelerator_provider,
    Browser* browser)
    : ui::SimpleMenuModel(this),
      browser_(browser),
      session_sync_service_(
          SessionSyncServiceFactory::GetInstance()->GetForProfile(
              browser->profile())),
      next_menu_id_(kFirstMenuEntryCommandId) {
  // Invoke asynchronous call to load tabs from local last session, which does
  // nothing if the tabs have already been loaded or they shouldn't be loaded.
  // TabRestoreServiceChanged() will be called after the tabs are loaded.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service) {
    service->LoadTabsFromLastSession();
    tab_restore_service_observation_.Observe(service);
  }

  if (session_sync_service_) {
    // Using a weak pointer below for simplicity although, strictly speaking,
    // it's not needed because the subscription itself should take care.
    foreign_session_updated_subscription_ =
        session_sync_service_->SubscribeToForeignSessionsChanged(
            base::BindRepeating(
                &RecentTabsSubMenuModel::OnForeignSessionUpdated,
                weak_ptr_factory_.GetWeakPtr()));
  }

  Build();

  if (accelerator_provider) {
    accelerator_provider->GetAcceleratorForCommandId(
        IDC_RESTORE_TAB, &reopen_closed_tab_accelerator_);
    accelerator_provider->GetAcceleratorForCommandId(
        IDC_SHOW_HISTORY, &show_history_accelerator_);
  }
}

RecentTabsSubMenuModel::~RecentTabsSubMenuModel() = default;

bool RecentTabsSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool RecentTabsSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return command_id != kDisabledRecentlyClosedHeaderCommandId &&
         command_id != IDC_RECENT_TABS_NO_DEVICE_TABS;
}

bool RecentTabsSubMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  // If there are no recently closed items, we show the accelerator beside
  // the header, otherwise, we show it beside the first item underneath it.
  // If the first item underneath it is a submenu, we instead show it beside
  // the first item in that submenu.
  const absl::optional<size_t> index_in_menu = GetIndexOfCommandId(command_id);
  const int parent_id = GetParentCommandId(command_id);
  const absl::optional<size_t> parent_index =
      parent_id == -1 ? absl::nullopt : GetIndexOfCommandId(parent_id);
  if ((command_id == kDisabledRecentlyClosedHeaderCommandId ||
       (recently_closed_title_index_.has_value() &&
        ((!IsSubMenuModelCommandId(command_id) &&
          index_in_menu == recently_closed_title_index_.value() + 1) ||
         ((command_id == local_window_items_.begin()->first ||
           command_id == local_group_items_.begin()->first) &&
          parent_index == recently_closed_title_index_.value() + 1)))) &&
      reopen_closed_tab_accelerator_.key_code() != ui::VKEY_UNKNOWN) {
    *accelerator = reopen_closed_tab_accelerator_;
    return true;
  }

  if (command_id == IDC_SHOW_HISTORY) {
    *accelerator = show_history_accelerator_;
    return true;
  }

  return false;
}

void RecentTabsSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction",
                             menu_opened_timer_.Elapsed());
  if (command_id == IDC_SHOW_HISTORY) {
    UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu", SHOW_MORE,
                              LIMIT_RECENT_TAB_ACTION);
    UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ShowHistory",
                               menu_opened_timer_.Elapsed());
    LogWrenchMenuAction(MENU_ACTION_SHOW_HISTORY);
    // We show all "other devices" on the history page.
    chrome::ExecuteCommandWithDisposition(
        browser_, IDC_SHOW_HISTORY, ui::DispositionFromEventFlags(event_flags));
    return;
  }
  if (command_id == IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS) {
    UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.LoginForDeviceTabs",
                               menu_opened_timer_.Elapsed());
    chrome::ExecuteCommandWithDisposition(
        browser_, IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS,
        ui::DispositionFromEventFlags(event_flags));
    return;
  }

  DCHECK_NE(IDC_RECENT_TABS_NO_DEVICE_TABS, command_id);

  WindowOpenDisposition disposition = ui::DispositionFromEventFlags(
      event_flags, WindowOpenDisposition::NEW_FOREGROUND_TAB);

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  sessions::LiveTabContext* context =
      BrowserLiveTabContext::FindContextForWebContents(
          browser_->tab_strip_model()->GetActiveWebContents());
  if (IsTabModelCommandId(command_id)) {
    TabNavigationItems* tab_items = GetTabVectorForCommandId(command_id);
    const TabNavigationItem& item = (*tab_items)[command_id];
    DCHECK(item.tab_id.is_valid() && item.url.is_valid());

    if (item.session_tag.empty()) {  // Restore tab of local session.
      if (service && context) {
        base::RecordAction(
            base::UserMetricsAction("WrenchMenu_OpenRecentTabFromLocal"));
        UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu",
                                  LOCAL_SESSION_TAB, LIMIT_RECENT_TAB_ACTION);
        service->RestoreEntryById(context, item.tab_id, disposition);
      }
    } else {  // Restore tab of session from other devices.
      sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
      if (!open_tabs)
        return;
      const sessions::SessionTab* tab;
      if (!open_tabs->GetForeignTab(item.session_tag, item.tab_id, &tab))
        return;
      if (tab->navigations.empty())
        return;
      base::RecordAction(
          base::UserMetricsAction("WrenchMenu_OpenRecentTabFromDevice"));
      UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu",
                                OTHER_DEVICE_TAB, LIMIT_RECENT_TAB_ACTION);
      SessionRestore::RestoreForeignSessionTab(
          browser_->tab_strip_model()->GetActiveWebContents(), *tab,
          disposition);
    }
  } else if (IsWindowModelCommandId(command_id)) {
    if (service && context) {
      base::RecordAction(
          base::UserMetricsAction("WrenchMenu_OpenRecentWindow"));
      UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu", RESTORE_WINDOW,
                                LIMIT_RECENT_TAB_ACTION);
      service->RestoreEntryById(context, local_window_items_.at(command_id),
                                disposition);
    }
  } else if (IsGroupModelCommandId(command_id)) {
    base::RecordAction(base::UserMetricsAction("WrenchMenu_OpenRecentGroup"));
    UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu", RESTORE_GROUP,
                              LIMIT_RECENT_TAB_ACTION);
    service->RestoreEntryById(context, local_group_items_.at(command_id),
                              disposition);
  } else {
    CHECK(IsSubMenuModelCommandId(command_id));
    return;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.OpenRecentTab",
                             menu_opened_timer_.Elapsed());
  UMA_HISTOGRAM_ENUMERATION("WrenchMenu.MenuAction", MENU_ACTION_RECENT_TAB,
                            LIMIT_MENU_ACTION);
}

int RecentTabsSubMenuModel::GetFirstRecentTabsCommandId() {
  return local_window_items_.begin()->first;
}

void RecentTabsSubMenuModel::Build() {
  // The menu contains:
  // - History to open the full history tab.
  // - Separator
  // - Recently closed header, then list of local recently closed tabs/windows,
  //   then separator
  // - device 1 section header, then list of tabs from device, then separator
  // - device 2 section header, then list of tabs from device, then separator
  // - device 3 section header, then list of tabs from device, then separator
  // |local_tab_navigation_items_| and |other_devices_tab_navigation_items_|
  // only contain navigatable (and hence executable) tab items for local
  // recently closed tabs and tabs from other devices respectively.
  // |local_window_items_| contains the local recently closed windows.
  InsertItemWithStringIdAt(0, IDC_SHOW_HISTORY, IDS_HISTORY_SHOW_HISTORY);
  if (features::IsChromeRefresh2023()) {
    SetCommandIcon(this, IDC_SHOW_HISTORY, kHistoryIcon);
  }
  InsertSeparatorAt(1, ui::NORMAL_SEPARATOR);
  BuildLocalEntries();
  BuildTabsFromOtherDevices();
}

void RecentTabsSubMenuModel::BuildLocalEntries() {
  last_local_model_index_ = kHistorySeparatorIndex;

  // All local items use InsertItem*At() to append or insert a menu item.
  // We're appending if building the entries for the first time i.e. invoked
  // from Constructor(), inserting when local entries change subsequently i.e.
  // invoked from TabRestoreServiceChanged().
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());

  if (!service || service->entries().empty()) {
    // This is to show a disabled restore tab entry with the accelerator to
    // teach users about this command.
    InsertItemWithStringIdAt(++last_local_model_index_,
                             kDisabledRecentlyClosedHeaderCommandId,
                             IDS_RECENTLY_CLOSED);
  } else {
    recently_closed_title_index_ = ++last_local_model_index_;
    InsertTitleWithStringIdAt(recently_closed_title_index_.value(),
                              IDS_RECENTLY_CLOSED);
    SetIcon(last_local_model_index_, CreateFavicon(kTabIcon));

    int added_count = 0;
    for (const auto& entry : service->entries()) {
      if (added_count == kMaxLocalEntries)
        break;
      switch (entry->type) {
        case sessions::TabRestoreService::TAB: {
          auto& tab =
              static_cast<const sessions::TabRestoreService::Tab&>(*entry);
          const sessions::SerializedNavigationEntry& current_navigation =
              tab.navigations.at(tab.current_navigation_index);
          BuildLocalTabItem(
              entry->id, tab.group_visual_data, current_navigation.title(),
              current_navigation.virtual_url(), ++last_local_model_index_);
          break;
        }
        case sessions::TabRestoreService::WINDOW: {
          auto& window =
              static_cast<sessions::TabRestoreService::Window&>(*entry);
          BuildLocalWindowItem(entry->id, CreateWindowSubMenuModel(window),
                               window.tabs.size(), ++last_local_model_index_);
          break;
        }
        case sessions::TabRestoreService::GROUP: {
          auto& group =
              static_cast<const sessions::TabRestoreService::Group&>(*entry);
          BuildLocalGroupItem(group.id, group.visual_data,
                              CreateGroupSubMenuModel(group), group.tabs.size(),
                              ++last_local_model_index_);
          break;
        }
      }
      ++added_count;
    }
  }
  DCHECK_GE(last_local_model_index_, 0u);
}

void RecentTabsSubMenuModel::BuildTabsFromOtherDevices() {
  // All other devices' items (device headers or tabs) use AddItem*() to append
  // a menu item, because they take always place in the end of menu.

  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  std::vector<const sync_sessions::SyncedSession*> sessions;
  if (!open_tabs || !open_tabs->GetAllForeignSessions(&sessions)) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    if (open_tabs || !features::IsChromeRefresh2023()) {
      AddItemWithStringId(IDC_RECENT_TABS_NO_DEVICE_TABS,
                          IDS_RECENT_TABS_NO_DEVICE_TABS);
    } else {
      AddItemWithStringIdAndIcon(IDC_RECENT_TABS_LOGIN_FOR_DEVICE_TABS,
                                 IDS_RECENT_TABS_LOGIN_FOR_DEVICE_TABS,
                                 ui::ImageModel::FromVectorIcon(
                                     kSyncRefreshIcon, ui::kColorMenuIcon,
                                     ui::SimpleMenuModel::kDefaultIconSize));
    }
    return;
  }

  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);

  const size_t kMaxSessionsToShow = 3;
  size_t num_sessions_added = 0;
  for (size_t i = 0;
       i < sessions.size() && num_sessions_added < kMaxSessionsToShow; ++i) {
    const sync_sessions::SyncedSession* session = sessions[i];
    const std::string& session_tag = session->GetSessionTag();

    // Collect tabs from all windows of the session, ordered by recency.
    std::vector<const sessions::SessionTab*> tabs_in_session;
    if (!open_tabs->GetForeignSessionTabs(session_tag, &tabs_in_session) ||
        tabs_in_session.empty())
      continue;

    // Add the header for the device session.
    DCHECK(!session->GetSessionName().empty());
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddTitle(base::UTF8ToUTF16(session->GetSessionName()));
    AddDeviceFavicon(GetItemCount() - 1, session->GetDeviceFormFactor());

    // Build tab menu items from sorted session tabs.
    const size_t kMaxTabsPerSessionToShow = 4;
    for (size_t k = 0;
         k < std::min(tabs_in_session.size(), kMaxTabsPerSessionToShow); ++k) {
      BuildOtherDevicesTabItem(session_tag, *tabs_in_session[k]);
    }  // for all tabs in one session
    ++num_sessions_added;
  }  // for all sessions

  // We are not supposed to get here unless at least some items were added.
  DCHECK_GT(GetItemCount(), 0u);
}

void RecentTabsSubMenuModel::BuildLocalTabItem(
    SessionID session_id,
    absl::optional<tab_groups::TabGroupVisualData> visual_data,
    const std::u16string& title,
    const GURL& url,
    size_t curr_model_index) {
  TabNavigationItem item(std::string(), session_id, title, url);
  const int command_id = GetAndIncrementNextMenuID();
  // See comments in BuildLocalEntries() about usage of InsertItem*At().
  // There may be no tab title, in which case, use the url as tab title.
  InsertItemAt(curr_model_index, command_id,
               title.empty() ? base::UTF8ToUTF16(item.url.spec()) : title);
  local_tab_navigation_items_.emplace(command_id, item);
  AddTabFavicon(command_id, this, item.url);
  // We shouldn't get here if there is no recently closed title.
  DCHECK(recently_closed_title_index_.has_value());
  // visual_data should only be populated if the tab was part of a tab group
  // when closed. We shouldn't set the minor icon for the item most recently
  // closed, as this creates visual clutter alongside the shortcut text.
  if (visual_data.has_value() &&
      curr_model_index > recently_closed_title_index_.value() + 1) {
    const ui::ColorProvider* color_provider =
        browser_->window()->GetColorProvider();
    const ui::ColorId color_id =
        GetTabGroupContextMenuColorId(visual_data.value().color());
    constexpr int kIconSize = 12;
    SetMinorIcon(
        curr_model_index,
        ui::ImageModel::FromVectorIcon(
            kTabGroupIcon, color_provider->GetColor(color_id), kIconSize));
  }
}

void RecentTabsSubMenuModel::BuildLocalWindowItem(
    SessionID window_id,
    std::unique_ptr<ui::SimpleMenuModel> window_model,
    size_t num_tabs,
    size_t curr_model_index) {
  const int command_id = GetAndIncrementNextMenuID();
  InsertSubMenuAt(curr_model_index, command_id,
                  l10n_util::GetPluralStringFUTF16(IDS_RECENTLY_CLOSED_WINDOW,
                                                   static_cast<int>(num_tabs)),
                  window_model.get());
  local_sub_menu_items_.emplace(
      command_id, SubMenuItem(command_id, std::move(window_model)));
  SetIcon(curr_model_index, CreateFavicon(kTabIcon));
}

void RecentTabsSubMenuModel::BuildLocalGroupItem(
    SessionID session_id,
    tab_groups::TabGroupVisualData visual_data,
    std::unique_ptr<ui::SimpleMenuModel> group_model,
    size_t num_tabs,
    size_t curr_model_index) {
  const int command_id = GetAndIncrementNextMenuID();
  // Set the item label to the name of the group and the number of tabs.
  std::u16string item_label = GetGroupItemLabel(visual_data.title(), num_tabs);
  InsertSubMenuAt(curr_model_index, command_id, item_label, group_model.get());
  local_sub_menu_items_.emplace(
      command_id, SubMenuItem(command_id, std::move(group_model)));

  // Set the item icon to the group color.
  const ui::ColorProvider* color_provider =
      browser_->window()->GetColorProvider();
  const ui::ColorId color_id =
      GetTabGroupContextMenuColorId(visual_data.color());
  ui::ImageModel group_icon = ui::ImageModel::FromVectorIcon(
      kTabGroupIcon, color_provider->GetColor(color_id), gfx::kFaviconSize);
  SetIcon(curr_model_index, group_icon);
}

void RecentTabsSubMenuModel::BuildOtherDevicesTabItem(
    const std::string& session_tag,
    const sessions::SessionTab& tab) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.normalized_navigation_index());
  TabNavigationItem item(session_tag, tab.tab_id, current_navigation.title(),
                         current_navigation.virtual_url());
  const int command_id = GetAndIncrementNextMenuID();
  // See comments in BuildTabsFromOtherDevices() about usage of AddItem*().
  // There may be no tab title, in which case, use the url as tab title.
  AddItem(command_id, current_navigation.title().empty()
                          ? base::UTF8ToUTF16(item.url.spec())
                          : current_navigation.title());
  other_devices_tab_navigation_items_.emplace(command_id, item);
  AddTabFavicon(command_id, this, item.url);
}

std::unique_ptr<ui::SimpleMenuModel>
RecentTabsSubMenuModel::CreateWindowSubMenuModel(
    const sessions::TabRestoreService::Window& window) {
  std::unique_ptr<ui::SimpleMenuModel> window_model =
      std::make_unique<ui::SimpleMenuModel>(this);
  const int restore_all_command_id = GetAndIncrementNextMenuID();
  window_model->AddItemWithStringIdAndIcon(
      restore_all_command_id, IDS_RESTORE_WINDOW,
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon));
  local_window_items_.emplace(restore_all_command_id, window.id);

  absl::optional<tab_groups::TabGroupId> last_group;
  tab_groups::TabGroupVisualData current_group_visual_data;
  std::unique_ptr<ui::SimpleMenuModel> current_group_model;
  for (const std::unique_ptr<sessions::TabRestoreService::Tab>& tab :
       window.tabs) {
    const sessions::SerializedNavigationEntry& current_navigation =
        tab->navigations.at(tab->current_navigation_index);
    const int tab_command_id = GetAndIncrementNextMenuID();
    if (tab->group != last_group && current_group_model) {
      AddGroupItemToModel(window_model.get(), std::move(current_group_model),
                          current_group_visual_data);
    }
    if (tab->group.has_value()) {
      if (!current_group_model) {
        last_group = tab->group.value();
        current_group_visual_data = tab->group_visual_data.value();
        current_group_model = std::make_unique<ui::SimpleMenuModel>(this);
        // TODO(emshack): Add restore all menu item as first item in model.
      }
      // The following comment silences a bugprone-use-after-move warning,
      // which does not apply to this use case as current_group_model will
      // always be set to a new value before adding an item to it.
      // NOLINTNEXTLINE
      current_group_model->AddItem(
          tab_command_id,
          current_navigation.title().empty()
              ? base::UTF8ToUTF16(current_navigation.virtual_url().spec())
              : current_navigation.title());
      local_group_items_.emplace(tab_command_id, tab->id);
      AddTabFavicon(tab_command_id, current_group_model.get(),
                    current_navigation.virtual_url());
    } else {
      // There may be no tab title, in which case, use the url as tab title.
      window_model->AddItem(
          tab_command_id,
          current_navigation.title().empty()
              ? base::UTF8ToUTF16(current_navigation.virtual_url().spec())
              : current_navigation.title());
      local_window_items_.emplace(tab_command_id, tab->id);
      AddTabFavicon(tab_command_id, window_model.get(),
                    current_navigation.virtual_url());
    }
  }
  if (current_group_model) {
    AddGroupItemToModel(window_model.get(), std::move(current_group_model),
                        current_group_visual_data);
  }
  return window_model;
}

std::unique_ptr<ui::SimpleMenuModel>
RecentTabsSubMenuModel::CreateGroupSubMenuModel(
    const sessions::TabRestoreService::Group& group) {
  std::unique_ptr<ui::SimpleMenuModel> group_model =
      std::make_unique<ui::SimpleMenuModel>(this);
  int command_id = GetAndIncrementNextMenuID();
  group_model->AddItemWithStringIdAndIcon(
      command_id, IDS_RESTORE_GROUP,
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon));
  local_group_items_.emplace(command_id, group.id);
  for (auto& tab : group.tabs) {
    const sessions::SerializedNavigationEntry& current_navigation =
        tab->navigations.at(tab->current_navigation_index);
    command_id = GetAndIncrementNextMenuID();
    // There may be no tab title, in which case, use the url as tab title.
    group_model->AddItem(
        command_id,
        current_navigation.title().empty()
            ? base::UTF8ToUTF16(current_navigation.virtual_url().spec())
            : current_navigation.title());
    local_group_items_.emplace(command_id, tab->id);
    AddTabFavicon(command_id, group_model.get(),
                  current_navigation.virtual_url());
  }
  return group_model;
}

void RecentTabsSubMenuModel::AddGroupItemToModel(
    SimpleMenuModel* parent_model,
    std::unique_ptr<SimpleMenuModel> group_model,
    tab_groups::TabGroupVisualData group_visual_data) {
  const int sub_menu_command_id = GetAndIncrementNextMenuID();
  const std::u16string sub_menu_label =
      GetGroupItemLabel(group_visual_data.title(), group_model->GetItemCount());
  // Set the item icon to the group color.
  const ui::ColorProvider* color_provider =
      browser_->window()->GetColorProvider();
  const ui::ColorId color_id =
      GetTabGroupContextMenuColorId(group_visual_data.color());
  ui::ImageModel group_icon = ui::ImageModel::FromVectorIcon(
      kTabGroupIcon, color_provider->GetColor(color_id), gfx::kFaviconSize);
  parent_model->AddSubMenuWithIcon(sub_menu_command_id, sub_menu_label,
                                   group_model.get(), group_icon);
  local_sub_menu_items_.emplace(
      sub_menu_command_id,
      SubMenuItem(sub_menu_command_id, std::move(group_model)));
}

std::u16string RecentTabsSubMenuModel::GetGroupItemLabel(std::u16string title,
                                                         size_t num_tabs) {
  std::u16string item_label;
  if (title.empty()) {
    item_label = l10n_util::GetPluralStringFUTF16(
        IDS_RECENTLY_CLOSED_GROUP_UNNAMED, static_cast<int>(num_tabs));
  } else {
    item_label = l10n_util::GetPluralStringFUTF16(IDS_RECENTLY_CLOSED_GROUP,
                                                  static_cast<int>(num_tabs));
    item_label = base::ReplaceStringPlaceholders(item_label, {title}, nullptr);
  }
  return item_label;
}

int RecentTabsSubMenuModel::GetParentCommandId(int command_id) const {
  for (auto& sub_menu_item : local_sub_menu_items_) {
    if (sub_menu_item.second.child_ids.find(command_id) !=
        sub_menu_item.second.child_ids.end()) {
      return sub_menu_item.second.parent_id;
    }
  }
  return -1;
}

void RecentTabsSubMenuModel::AddDeviceFavicon(
    size_t index_in_menu,
    syncer::DeviceInfo::FormFactor device_form_factor) {
  const gfx::VectorIcon* favicon = nullptr;
  switch (device_form_factor) {
    case syncer::DeviceInfo::FormFactor::kPhone:
      favicon = &kSmartphoneIcon;
      break;
    case syncer::DeviceInfo::FormFactor::kTablet:
      favicon = &kTabletIcon;
      break;
    case syncer::DeviceInfo::FormFactor::kUnknown:
      [[fallthrough]];  // Return the laptop icon as default.
    case syncer::DeviceInfo::FormFactor::kDesktop:
      favicon = &kLaptopIcon;
      break;
  }

  SetIcon(index_in_menu, CreateFavicon(*favicon));
}

void RecentTabsSubMenuModel::AddTabFavicon(int command_id,
                                           ui::SimpleMenuModel* menu_model,
                                           const GURL& url) {
  const size_t index_in_menu =
      menu_model->GetIndexOfCommandId(command_id).value();
  // Set default icon first.
  menu_model->SetIcon(index_in_menu, favicon::GetDefaultFaviconModel());

  const bool is_local_tab = IsLocalTabModelCommandId(command_id) ||
                            IsWindowModelCommandId(command_id) ||
                            IsGroupModelCommandId(command_id);
  if (is_local_tab) {
    // Request only from local storage to avoid leaking user data.
    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            browser_->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    // Can be null for tests.
    if (!favicon_service)
      return;
    favicon_service->GetFaviconImageForPageURL(
        url,
        base::BindOnce(&RecentTabsSubMenuModel::OnFaviconDataAvailable,
                       weak_ptr_factory_.GetWeakPtr(), command_id, menu_model),
        &local_tab_cancelable_task_tracker_);
  } else {
    favicon::HistoryUiFaviconRequestHandler*
        history_ui_favicon_request_handler =
            HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(
                browser_->profile());
    // Can be null for tests.
    if (!history_ui_favicon_request_handler)
      return;
    history_ui_favicon_request_handler->GetFaviconImageForPageURL(
        url,
        base::BindOnce(&RecentTabsSubMenuModel::OnFaviconDataAvailable,
                       weak_ptr_factory_for_other_devices_tab_.GetWeakPtr(),
                       command_id, menu_model),

        favicon::HistoryUiFaviconRequestOrigin::kRecentTabs);
  }
}

void RecentTabsSubMenuModel::OnFaviconDataAvailable(
    int command_id,
    ui::SimpleMenuModel* menu_model,
    const favicon_base::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty()) {
    // Default icon has already been set.
    return;
  }
  const absl::optional<size_t> index_in_menu =
      menu_model->GetIndexOfCommandId(command_id);
  DCHECK(index_in_menu.has_value());
  menu_model->SetIcon(index_in_menu.value(),
                      ui::ImageModel::FromImage(image_result.image));
  ui::MenuModelDelegate* delegate = menu_model_delegate();
  if (delegate)
    delegate->OnIconChanged(command_id);
  return;
}

RecentTabsSubMenuModel::TabNavigationItems*
RecentTabsSubMenuModel::GetTabVectorForCommandId(int command_id) {
  DCHECK(IsTabModelCommandId(command_id));
  return IsOtherDeviceTabModelCommandId(command_id)
             ? &other_devices_tab_navigation_items_
             : &local_tab_navigation_items_;
}

void RecentTabsSubMenuModel::ClearLocalEntries() {
  // Remove local items (recently closed tabs and windows) from menumodel.
  while (last_local_model_index_ > kHistorySeparatorIndex) {
    RemoveItemAt(last_local_model_index_--);
  }
  recently_closed_title_index_.reset();

  // Cancel asynchronous FaviconService::GetFaviconImageForPageURL() tasks of
  // all local tabs.
  local_tab_cancelable_task_tracker_.TryCancelAll();

  // Remove all local tab navigation items.
  local_tab_navigation_items_.clear();

  // Remove all local window items.
  local_window_items_.clear();

  // Remove all local sub menu items.
  local_sub_menu_items_.clear();
}

void RecentTabsSubMenuModel::ClearTabsFromOtherDevices() {
  for (size_t index = GetItemCount(); index > last_local_model_index_ + 1;
       --index) {
    RemoveItemAt(index - 1);
  }

  weak_ptr_factory_for_other_devices_tab_.InvalidateWeakPtrs();

  other_devices_tab_navigation_items_.clear();
}

sync_sessions::OpenTabsUIDelegate*
RecentTabsSubMenuModel::GetOpenTabsUIDelegate() {
  DCHECK(session_sync_service_);
  return session_sync_service_->GetOpenTabsUIDelegate();
}

void RecentTabsSubMenuModel::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  ClearLocalEntries();

  BuildLocalEntries();

  ui::MenuModelDelegate* delegate = menu_model_delegate();
  if (delegate)
    delegate->OnMenuStructureChanged();
}

void RecentTabsSubMenuModel::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  TabRestoreServiceChanged(service);
}

void RecentTabsSubMenuModel::OnForeignSessionUpdated() {
  ClearTabsFromOtherDevices();

  BuildTabsFromOtherDevices();

  ui::MenuModelDelegate* delegate = menu_model_delegate();
  if (delegate)
    delegate->OnMenuStructureChanged();
}

int RecentTabsSubMenuModel::GetAndIncrementNextMenuID() {
  const int current_id = next_menu_id_;
  next_menu_id_ += AppMenuModel::kNumUnboundedMenuTypes;
  return current_id;
}

bool RecentTabsSubMenuModel::IsTabModelCommandId(int command_id) const {
  return IsLocalTabModelCommandId(command_id) ||
         IsOtherDeviceTabModelCommandId(command_id);
}

bool RecentTabsSubMenuModel::IsLocalTabModelCommandId(int command_id) const {
  return local_tab_navigation_items_.count(command_id) > 0;
}

bool RecentTabsSubMenuModel::IsOtherDeviceTabModelCommandId(
    int command_id) const {
  return other_devices_tab_navigation_items_.count(command_id) > 0;
}

bool RecentTabsSubMenuModel::IsWindowModelCommandId(int command_id) const {
  return local_window_items_.count(command_id) > 0;
}

bool RecentTabsSubMenuModel::IsGroupModelCommandId(int command_id) const {
  return local_group_items_.count(command_id) > 0;
}

bool RecentTabsSubMenuModel::IsSubMenuModelCommandId(int command_id) const {
  return local_sub_menu_items_.count(command_id) > 0;
}
