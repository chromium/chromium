// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"

#include <stddef.h>

#include <algorithm>

#include "base/bind.h"
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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/history_ui_favicon_request_handler.h"
#include "components/favicon_base/favicon_types.h"
#include "components/feature_engagement/buildflags.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// Initial comamnd ID's for navigatable (and hence executable) tab/window menu
// items.  The menumodel and storage structures are not 1-1:
// - menumodel has "Recently closed" header, "No tabs from other devices",
//   device section headers, separators, local and other devices' tab items, and
//   local window items.
// - |local_tab_navigation_items_| and |other_devices_tab_navigation_items_|
// only have navigatabale/executable tab items.
// - |local_window_items_| only has executable open window items.
// Using initial command IDs for local tab, local window and other devices' tab
// items makes it easier and less error-prone to manipulate the menumodel and
// storage structures.  These ids must be bigger than the maximum possible
// number of items in the menumodel, so that index of the last menu item doesn't
// clash with these values when menu items are retrieved via
// GetIndexOfCommandId().
// The range of all command ID's used in RecentTabsSubMenuModel, including the
// "Recently closed" headers, must be between
// |AppMenuModel::kMinRecentTabsCommandId| i.e. 1001 and 1200
// (|AppMenuModel::kMaxRecentTabsCommandId|) inclusively.
const int kFirstLocalTabCommandId = AppMenuModel::kMinRecentTabsCommandId;
const int kFirstLocalWindowCommandId = 1031;
const int kFirstOtherDevicesTabCommandId = 1051;
const int kMinDeviceNameCommandId = 1100;
const int kMaxDeviceNameCommandId = 1110;

// The maximum number of local recently closed entries (tab or window) to be
// shown in the menu.
const int kMaxLocalEntries = 8;

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SortSessionsByRecency(const sync_sessions::SyncedSession* s1,
                           const sync_sessions::SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

// Returns true if the command id identifies a tab menu item.
bool IsTabModelCommandId(int command_id) {
  return ((command_id >= kFirstLocalTabCommandId &&
           command_id < kFirstLocalWindowCommandId) ||
          (command_id >= kFirstOtherDevicesTabCommandId &&
           command_id < kMinDeviceNameCommandId));
}

// Returns true if the command id identifies a window menu item.
bool IsWindowModelCommandId(int command_id) {
  return command_id >= kFirstLocalWindowCommandId &&
         command_id < kFirstOtherDevicesTabCommandId;
}

bool IsDeviceNameCommandId(int command_id) {
  return command_id >= kMinDeviceNameCommandId &&
      command_id <= kMaxDeviceNameCommandId;
}

// Convert |tab_vector_index| to command id of menu item, with
// |first_command_id| as the base command id.
int TabVectorIndexToCommandId(int tab_vector_index, int first_command_id) {
  int command_id = tab_vector_index + first_command_id;
  DCHECK(IsTabModelCommandId(command_id));
  return command_id;
}

// Convert |window_vector_index| to command id of menu item.
int WindowVectorIndexToCommandId(int window_vector_index) {
  int command_id = window_vector_index + kFirstLocalWindowCommandId;
  DCHECK(IsWindowModelCommandId(command_id));
  return command_id;
}

// Convert |command_id| of menu item to index in |local_window_items_|.
int CommandIdToWindowVectorIndex(int command_id) {
  DCHECK(IsWindowModelCommandId(command_id));
  return command_id - kFirstLocalWindowCommandId;
}

gfx::Image CreateFavicon(const gfx::VectorIcon& icon) {
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  return gfx::Image(
      gfx::CreateVectorIcon(icon, 16,
                            native_theme->GetSystemColor(
                                ui::NativeTheme::kColorId_DefaultIconColor)));
}

}  // namespace

enum RecentTabAction {
  LOCAL_SESSION_TAB = 0,
  OTHER_DEVICE_TAB,
  RESTORE_WINDOW,
  SHOW_MORE,
  LIMIT_RECENT_TAB_ACTION
};

// An element in |RecentTabsSubMenuModel::local_tab_navigation_items_| or
// |RecentTabsSubMenuModel::other_devices_tab_navigation_items_| that stores
// the navigation information of a local or other devices' tab required to
// restore the tab.
struct RecentTabsSubMenuModel::TabNavigationItem {
  TabNavigationItem() : tab_id(SessionID::InvalidValue()) {}

  TabNavigationItem(const std::string& session_tag,
                    SessionID tab_id,
                    const base::string16& title,
                    const GURL& url)
      : session_tag(session_tag), tab_id(tab_id), title(title), url(url) {}

  // For use by std::set for sorting.
  bool operator<(const TabNavigationItem& other) const {
    return url < other.url;
  }

  // Empty for local tabs, non-empty for other devices' tabs.
  std::string session_tag;
  SessionID tab_id;  // Might be invalid.
  base::string16 title;
  GURL url;
};

RecentTabsSubMenuModel::RecentTabsSubMenuModel(
    ui::AcceleratorProvider* accelerator_provider,
    Browser* browser)
    : ui::SimpleMenuModel(this),
      browser_(browser),
      session_sync_service_(
          SessionSyncServiceFactory::GetInstance()->GetForProfile(
              browser->profile())) {
  // Invoke asynchronous call to load tabs from local last session, which does
  // nothing if the tabs have already been loaded or they shouldn't be loaded.
  // TabRestoreServiceChanged() will be called after the tabs are loaded.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (service) {
    service->LoadTabsFromLastSession();
    tab_restore_service_observer_.Add(service);
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

RecentTabsSubMenuModel::~RecentTabsSubMenuModel() {}

bool RecentTabsSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool RecentTabsSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return command_id != kRecentlyClosedHeaderCommandId &&
         command_id != kDisabledRecentlyClosedHeaderCommandId &&
         command_id != IDC_RECENT_TABS_NO_DEVICE_TABS &&
         !IsDeviceNameCommandId(command_id);
}

bool RecentTabsSubMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  // If there are no recently closed items, we show the accelerator beside
  // the header, otherwise, we show it beside the first item underneath it.
  int index_in_menu = GetIndexOfCommandId(command_id);
  int header_index = GetIndexOfCommandId(kRecentlyClosedHeaderCommandId);
  if ((command_id == kDisabledRecentlyClosedHeaderCommandId ||
       (header_index != -1 && index_in_menu == header_index + 1)) &&
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
    chrome::ExecuteCommandWithDisposition(browser_, IDC_SHOW_HISTORY,
        ui::DispositionFromEventFlags(event_flags));
    return;
  }

  DCHECK_NE(IDC_RECENT_TABS_NO_DEVICE_TABS, command_id);
  DCHECK(!IsDeviceNameCommandId(command_id));

  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    // Force to open a new foreground tab.
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  sessions::LiveTabContext* context =
      BrowserLiveTabContext::FindContextForWebContents(
          browser_->tab_strip_model()->GetActiveWebContents());
  if (IsTabModelCommandId(command_id)) {
    TabNavigationItems* tab_items = NULL;
    int tab_items_idx = CommandIdToTabVectorIndex(command_id, &tab_items);
    const TabNavigationItem& item = (*tab_items)[tab_items_idx];
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
          browser_->tab_strip_model()->GetActiveWebContents(),
          *tab, disposition);
    }
  } else {
    DCHECK(IsWindowModelCommandId(command_id));
    if (service && context) {
      int window_items_idx = CommandIdToWindowVectorIndex(command_id);
      DCHECK(window_items_idx >= 0 &&
             window_items_idx < static_cast<int>(local_window_items_.size()));
      base::RecordAction(
          base::UserMetricsAction("WrenchMenu_OpenRecentWindow"));
      UMA_HISTOGRAM_ENUMERATION("WrenchMenu.RecentTabsSubMenu", RESTORE_WINDOW,
                                LIMIT_RECENT_TAB_ACTION);
      service->RestoreEntryById(context, local_window_items_[window_items_idx],
                                disposition);
    }
  }

  browser_->window()->OnTabRestored(command_id);

  UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.OpenRecentTab",
                             menu_opened_timer_.Elapsed());
  UMA_HISTOGRAM_ENUMERATION("WrenchMenu.MenuAction", MENU_ACTION_RECENT_TAB,
                             LIMIT_MENU_ACTION);
}

int RecentTabsSubMenuModel::GetFirstRecentTabsCommandId() {
  return WindowVectorIndexToCommandId(0);
}

const gfx::FontList* RecentTabsSubMenuModel::GetLabelFontListAt(
    int index) const {
  int command_id = GetCommandIdAt(index);
  if (command_id == kRecentlyClosedHeaderCommandId ||
      IsDeviceNameCommandId(command_id)) {
    return &ui::ResourceBundle::GetSharedInstance().GetFontList(
        ui::ResourceBundle::BoldFont);
  }
  return NULL;
}

int RecentTabsSubMenuModel::GetMaxWidthForItemAtIndex(int item_index) const {
  int command_id = GetCommandIdAt(item_index);
  if (command_id == IDC_RECENT_TABS_NO_DEVICE_TABS ||
      command_id == kRecentlyClosedHeaderCommandId ||
      command_id == kDisabledRecentlyClosedHeaderCommandId) {
    return -1;
  }
  return 320;
}

bool RecentTabsSubMenuModel::GetURLAndTitleForItemAtIndex(
    int index,
    std::string* url,
    base::string16* title) {
  int command_id = GetCommandIdAt(index);
  if (IsTabModelCommandId(command_id)) {
    TabNavigationItems* tab_items = NULL;
    int tab_items_idx = CommandIdToTabVectorIndex(command_id, &tab_items);
    const TabNavigationItem& item = (*tab_items)[tab_items_idx];
    *url = item.url.possibly_invalid_spec();
    *title = item.title;
    return true;
  }
  return false;
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
    InsertItemWithStringIdAt(++last_local_model_index_,
                             kRecentlyClosedHeaderCommandId,
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
          BuildLocalTabItem(entry->id, current_navigation.title(),
                            current_navigation.virtual_url(),
                            ++last_local_model_index_);
          break;
        }
        case sessions::TabRestoreService::WINDOW: {
          // TODO(chrisha): Make this menu entry better. When windows contain a
          // single tab, display that tab directly in the menu. Otherwise, offer
          // a hover over or alternative mechanism for seeing which tabs were in
          // the window.
          BuildLocalWindowItem(
              entry->id,
              static_cast<const sessions::TabRestoreService::Window&>(*entry)
                  .tabs.size(),
              ++last_local_model_index_);
          break;
        }
      }
      ++added_count;
    }
  }
  DCHECK_GE(last_local_model_index_, 0);
}

void RecentTabsSubMenuModel::BuildTabsFromOtherDevices() {
  // All other devices' items (device headers or tabs) use AddItem*() to append
  // a menu item, because they take always place in the end of menu.

  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  std::vector<const sync_sessions::SyncedSession*> sessions;
  if (!open_tabs || !open_tabs->GetAllForeignSessions(&sessions)) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(IDC_RECENT_TABS_NO_DEVICE_TABS,
                        IDS_RECENT_TABS_NO_DEVICE_TABS);
    return;
  }

  // Sort sessions from most recent to least recent.
  std::sort(sessions.begin(), sessions.end(), SortSessionsByRecency);

  const size_t kMaxSessionsToShow = 3;
  size_t num_sessions_added = 0;
  for (size_t i = 0;
       i < sessions.size() && num_sessions_added < kMaxSessionsToShow; ++i) {
    const sync_sessions::SyncedSession* session = sessions[i];
    const std::string& session_tag = session->session_tag;

    // Collect tabs from all windows of the session, ordered by recency.
    std::vector<const sessions::SessionTab*> tabs_in_session;
    if (!open_tabs->GetForeignSessionTabs(session_tag, &tabs_in_session) ||
        tabs_in_session.empty())
      continue;

    // Add the header for the device session.
    DCHECK(!session->session_name.empty());
    AddSeparator(ui::NORMAL_SEPARATOR);
    int command_id = kMinDeviceNameCommandId + i;
    DCHECK_LE(command_id, kMaxDeviceNameCommandId);
    AddItem(command_id, base::UTF8ToUTF16(session->session_name));
    AddDeviceFavicon(GetItemCount() - 1, session->device_type);

    // Build tab menu items from sorted session tabs.
    const size_t kMaxTabsPerSessionToShow = 4;
    for (size_t k = 0;
         k < std::min(tabs_in_session.size(), kMaxTabsPerSessionToShow);
         ++k) {
      BuildOtherDevicesTabItem(session_tag, *tabs_in_session[k]);
    }  // for all tabs in one session

    ++num_sessions_added;
  }  // for all sessions

  // We are not supposed to get here unless at least some items were added.
  DCHECK_GT(GetItemCount(), 0);
}

void RecentTabsSubMenuModel::BuildLocalTabItem(SessionID session_id,
                                               const base::string16& title,
                                               const GURL& url,
                                               int curr_model_index) {
  TabNavigationItem item(std::string(), session_id, title, url);
  int command_id = TabVectorIndexToCommandId(
      local_tab_navigation_items_.size(), kFirstLocalTabCommandId);
  // See comments in BuildLocalEntries() about usage of InsertItem*At().
  // There may be no tab title, in which case, use the url as tab title.
  InsertItemAt(curr_model_index, command_id,
               title.empty() ? base::UTF8ToUTF16(item.url.spec()) : title);
  AddTabFavicon(command_id, item.url);
  local_tab_navigation_items_.push_back(item);
}

void RecentTabsSubMenuModel::BuildLocalWindowItem(SessionID window_id,
                                                  int num_tabs,
                                                  int curr_model_index) {
  int command_id = WindowVectorIndexToCommandId(local_window_items_.size());
  // See comments in BuildLocalEntries() about usage of InsertItem*At().
  InsertItemAt(curr_model_index, command_id, l10n_util::GetPluralStringFUTF16(
      IDS_RECENTLY_CLOSED_WINDOW, num_tabs));
  SetIcon(curr_model_index, CreateFavicon(kTabIcon));
  local_window_items_.push_back(window_id);
}

void RecentTabsSubMenuModel::BuildOtherDevicesTabItem(
    const std::string& session_tag,
    const sessions::SessionTab& tab) {
  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(tab.normalized_navigation_index());
  TabNavigationItem item(session_tag, tab.tab_id, current_navigation.title(),
                         current_navigation.virtual_url());
  int command_id = TabVectorIndexToCommandId(
      other_devices_tab_navigation_items_.size(),
      kFirstOtherDevicesTabCommandId);
  // See comments in BuildTabsFromOtherDevices() about usage of AddItem*().
  // There may be no tab title, in which case, use the url as tab title.
  AddItem(command_id,
          current_navigation.title().empty() ?
              base::UTF8ToUTF16(item.url.spec()) : current_navigation.title());
  AddTabFavicon(command_id, item.url);
  other_devices_tab_navigation_items_.push_back(item);
}

void RecentTabsSubMenuModel::AddDeviceFavicon(
    int index_in_menu,
    sync_pb::SyncEnums::DeviceType device_type) {
  const gfx::VectorIcon* favicon = nullptr;
  switch (device_type) {
    case sync_pb::SyncEnums::TYPE_PHONE:
      favicon = &kSmartphoneIcon;
      break;

    case sync_pb::SyncEnums::TYPE_TABLET:
      favicon = &kTabletIcon;
      break;

    case sync_pb::SyncEnums::TYPE_CROS:
    case sync_pb::SyncEnums::TYPE_WIN:
    case sync_pb::SyncEnums::TYPE_MAC:
    case sync_pb::SyncEnums::TYPE_LINUX:
    case sync_pb::SyncEnums::TYPE_OTHER:
    case sync_pb::SyncEnums::TYPE_UNSET:
      favicon = &kLaptopIcon;
      break;
  }

  SetIcon(index_in_menu, CreateFavicon(*favicon));
}

void RecentTabsSubMenuModel::AddTabFavicon(int command_id, const GURL& url) {
  int index_in_menu = GetIndexOfCommandId(command_id);

  // Set default icon first.
  SetIcon(index_in_menu, favicon::GetDefaultFavicon());

  bool is_local_tab = command_id < kFirstOtherDevicesTabCommandId;
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
        base::Bind(&RecentTabsSubMenuModel::OnFaviconDataAvailable,
                   weak_ptr_factory_.GetWeakPtr(), command_id),
        &local_tab_cancelable_task_tracker_);
  } else {
    favicon::HistoryUiFaviconRequestHandler*
        history_ui_favicon_request_handler =
            HistoryUiFaviconRequestHandlerFactory::GetForBrowserContext(
                browser_->profile());
    // Can be null for tests.
    if (!history_ui_favicon_request_handler)
      return;
    sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
    history_ui_favicon_request_handler->GetFaviconImageForPageURL(
        url,
        base::BindOnce(&RecentTabsSubMenuModel::OnFaviconDataAvailable,
                       weak_ptr_factory_.GetWeakPtr(), command_id),

        favicon::HistoryUiFaviconRequestOrigin::kRecentTabs,
        open_tabs ? open_tabs->GetIconUrlForPageUrl(url) : GURL(),
        &other_devices_tab_cancelable_task_tracker_);
  }
}

void RecentTabsSubMenuModel::OnFaviconDataAvailable(
    int command_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty()) {
    // Default icon has already been set.
    return;
  }
  int index_in_menu = GetIndexOfCommandId(command_id);
  DCHECK_GT(index_in_menu, -1);
  SetIcon(index_in_menu, image_result.image);
  ui::MenuModelDelegate* delegate = menu_model_delegate();
  if (delegate)
    delegate->OnIconChanged(index_in_menu);
  return;
}

int RecentTabsSubMenuModel::CommandIdToTabVectorIndex(
    int command_id,
    TabNavigationItems** tab_items) {
  DCHECK(IsTabModelCommandId(command_id));
  if (command_id >= kFirstOtherDevicesTabCommandId) {
    *tab_items = &other_devices_tab_navigation_items_;
    return command_id - kFirstOtherDevicesTabCommandId;
  }
  *tab_items = &local_tab_navigation_items_;
  return command_id - kFirstLocalTabCommandId;
}

void RecentTabsSubMenuModel::ClearLocalEntries() {
  // Remove local items (recently closed tabs and windows) from menumodel.
  while (last_local_model_index_ > kHistorySeparatorIndex)
    RemoveItemAt(last_local_model_index_--);

  // Cancel asynchronous FaviconService::GetFaviconImageForPageURL() tasks of
  // all local tabs.
  local_tab_cancelable_task_tracker_.TryCancelAll();

  // Remove all local tab navigation items.
  local_tab_navigation_items_.clear();

  // Remove all local window items.
  local_window_items_.clear();
}

void RecentTabsSubMenuModel::ClearTabsFromOtherDevices() {
  DCHECK_GE(last_local_model_index_, 0);
  int count = GetItemCount();
  for (int index = count - 1; index > last_local_model_index_; --index)
    RemoveItemAt(index);

  other_devices_tab_cancelable_task_tracker_.TryCancelAll();

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
