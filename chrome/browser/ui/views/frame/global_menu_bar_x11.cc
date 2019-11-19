// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/global_menu_bar_x11.h"

#include <dlfcn.h>
#include <glib-object.h>
#include <stddef.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/global_menu_bar_registrar_x11.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/history/core/browser/top_sites.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/strings/grit/components_strings.h"
#include "dbus/object_path.h"
#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/text_elider.h"

// A line in the static menu definitions.
struct GlobalMenuBarCommand {
  int command;
  int str_id;
};

namespace {

// The maximum number of most visited items to display.
const unsigned int kMostVisitedCount = 8;

// The number of recently closed items to get.
const unsigned int kRecentlyClosedCount = 8;

// Menus more than this many chars long will get trimmed.
const size_t kMaximumMenuWidthInChars = 50;

// Constants used in menu definitions.  The first non-Chrome command is at
// IDC_FIRST_BOOKMARK_MENU.
enum ReservedCommandId {
  kLastChromeCommand = IDC_FIRST_BOOKMARK_MENU - 1,
  kMenuEnd,
  kSeparator,
  kSubmenu,
  kTagRecentlyClosed,
  kTagMostVisited,
  kTagProfileEdit,
  kTagProfileCreate,
  kFirstUnreservedCommandId
};

constexpr GlobalMenuBarCommand kFileMenu[] = {
    {IDC_NEW_TAB, IDS_NEW_TAB},
    {IDC_NEW_WINDOW, IDS_NEW_WINDOW},
    {IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW},
    {IDC_RESTORE_TAB, IDS_REOPEN_CLOSED_TABS_LINUX},
    {IDC_OPEN_FILE, IDS_OPEN_FILE_LINUX},
    {IDC_FOCUS_LOCATION, IDS_OPEN_LOCATION_LINUX},
    {kSeparator},
    {IDC_CLOSE_WINDOW, IDS_CLOSE_WINDOW_LINUX},
    {IDC_CLOSE_TAB, IDS_CLOSE_TAB_LINUX},
    {IDC_SAVE_PAGE, IDS_SAVE_PAGE},
    {kSeparator},
    {IDC_PRINT, IDS_PRINT},
    {kMenuEnd}};

constexpr GlobalMenuBarCommand kEditMenu[] = {{IDC_CUT, IDS_CUT},
                                              {IDC_COPY, IDS_COPY},
                                              {IDC_PASTE, IDS_PASTE},
                                              {kSeparator},
                                              {IDC_FIND, IDS_FIND},
                                              {kSeparator},
                                              {IDC_OPTIONS, IDS_PREFERENCES},
                                              {kMenuEnd}};

constexpr GlobalMenuBarCommand kViewMenu[] = {
    {IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR},
    {kSeparator},
    {IDC_STOP, IDS_STOP_MENU_LINUX},
    {IDC_RELOAD, IDS_RELOAD_MENU_LINUX},
    {kSeparator},
    {IDC_FULLSCREEN, IDS_FULLSCREEN},
    {IDC_ZOOM_NORMAL, IDS_TEXT_DEFAULT_LINUX},
    {IDC_ZOOM_PLUS, IDS_TEXT_BIGGER_LINUX},
    {IDC_ZOOM_MINUS, IDS_TEXT_SMALLER_LINUX},
    {kMenuEnd}};

constexpr GlobalMenuBarCommand kHistoryMenu[] = {
    {IDC_HOME, IDS_HISTORY_HOME_LINUX},
    {IDC_BACK, IDS_HISTORY_BACK_LINUX},
    {IDC_FORWARD, IDS_HISTORY_FORWARD_LINUX},
    {kSeparator},
    {kTagRecentlyClosed, IDS_HISTORY_CLOSED_LINUX},
    {kSeparator},
    {kTagMostVisited, IDS_HISTORY_VISITED_LINUX},
    {kSeparator},
    {IDC_SHOW_HISTORY, IDS_HISTORY_SHOWFULLHISTORY_LINK},
    {kMenuEnd}};

constexpr GlobalMenuBarCommand kToolsMenu[] = {
    {IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS},
    {IDC_SHOW_HISTORY, IDS_HISTORY_SHOW_HISTORY},
    {IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS},
    {kSeparator},
    {IDC_TASK_MANAGER, IDS_TASK_MANAGER},
    {IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA},
    {kSeparator},
    {IDC_VIEW_SOURCE, IDS_VIEW_SOURCE},
    {IDC_DEV_TOOLS, IDS_DEV_TOOLS},
    {IDC_DEV_TOOLS_INSPECT, IDS_DEV_TOOLS_ELEMENTS},
    {IDC_DEV_TOOLS_CONSOLE, IDS_DEV_TOOLS_CONSOLE},
    {IDC_DEV_TOOLS_DEVICES, IDS_DEV_TOOLS_DEVICES},
    {kMenuEnd}};

constexpr GlobalMenuBarCommand kProfilesMenu[] = {
    {kSeparator},
    {kTagProfileEdit, IDS_PROFILES_MANAGE_BUTTON_LABEL},
    {kTagProfileCreate, IDS_PROFILES_CREATE_BUTTON_LABEL},
    {kMenuEnd}};

constexpr GlobalMenuBarCommand kHelpMenu[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {IDC_FEEDBACK, IDS_FEEDBACK},
#endif
    {IDC_HELP_PAGE_VIA_MENU, IDS_HELP_PAGE},
    {kMenuEnd}};

void FindMenuItemsForCommandAux(
    ui::MenuModel* menu,
    int command,
    std::vector<std::pair<ui::MenuModel*, int>>* menu_items) {
  for (int i = 0; i < menu->GetItemCount(); i++) {
    if (menu->GetCommandIdAt(i) == command)
      menu_items->push_back({menu, i});
    if (menu->GetTypeAt(i) == ui::SimpleMenuModel::ItemType::TYPE_SUBMENU) {
      FindMenuItemsForCommandAux(menu->GetSubmenuModelAt(i), command,
                                 menu_items);
    }
  }
}

std::vector<std::pair<ui::MenuModel*, int>> FindMenuItemsForCommand(
    ui::MenuModel* menu,
    int command) {
  std::vector<std::pair<ui::MenuModel*, int>> menu_items;
  FindMenuItemsForCommandAux(menu, command, &menu_items);
  return menu_items;
}

}  // namespace

struct GlobalMenuBarX11::HistoryItem {
  HistoryItem() : session_id(SessionID::InvalidValue()) {}

  // The title for the menu item.
  base::string16 title;
  // The URL that will be navigated to if the user selects this item.
  GURL url;

  // This ID is unique for a browser session and can be passed to the
  // TabRestoreService to re-open the closed window or tab that this
  // references. A valid session ID indicates that this is an entry can be
  // restored that way. Otherwise, the URL will be used to open the item and
  // this ID will be invalid.
  SessionID session_id;

  // If the HistoryItem is a window, this will be the vector of tabs. Note
  // that this is a list of weak references. GlobalMenuBarX11::history_items_
  // is the owner of all items. If it is not a window, then the entry is a
  // single page and the vector will be empty.
  std::vector<HistoryItem*> tabs;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryItem);
};

GlobalMenuBarX11::GlobalMenuBarX11(BrowserView* browser_view,
                                   aura::WindowTreeHost* host)
    : browser_(browser_view->browser()),
      profile_(browser_->profile()),
      browser_view_(browser_view),
      xid_(host->GetAcceleratedWidget()),
      tab_restore_service_(nullptr),
      last_command_id_(kFirstUnreservedCommandId - 1) {
  GlobalMenuBarRegistrarX11::GetInstance()->OnMenuBarCreated(this);
}

GlobalMenuBarX11::~GlobalMenuBarX11() {
  auto* registrar = GlobalMenuBarRegistrarX11::GetInstance();
  registrar->OnMenuBarDestroyed(this);

  if (!initialized_)
    return;

  registrar->bus()->UnregisterExportedObject(dbus::ObjectPath(GetPath()));

  for (int command : observed_commands_)
    chrome::RemoveCommandObserver(browser_, command, this);

  pref_change_registrar_.RemoveAll();

  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);

  BrowserList::RemoveObserver(this);
}

void GlobalMenuBarX11::Initialize(DbusMenu::InitializedCallback callback) {
  DCHECK(!initialized_);
  initialized_ = true;

  // First build static menu content.
  root_menu_ = std::make_unique<ui::SimpleMenuModel>(this);

  BuildStaticMenu(IDS_FILE_MENU_LINUX, kFileMenu);
  BuildStaticMenu(IDS_EDIT_MENU_LINUX, kEditMenu);
  BuildStaticMenu(IDS_VIEW_MENU_LINUX, kViewMenu);
  history_menu_ = BuildStaticMenu(IDS_HISTORY_MENU_LINUX, kHistoryMenu);
  BuildStaticMenu(IDS_TOOLS_MENU_LINUX, kToolsMenu);
  profiles_menu_ =
      BuildStaticMenu(IDS_PROFILES_OPTIONS_GROUP_NAME, kProfilesMenu);
  BuildStaticMenu(IDS_HELP_MENU_LINUX, kHelpMenu);

  pref_change_registrar_.Init(browser_->profile()->GetPrefs());
  pref_change_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::Bind(&GlobalMenuBarX11::OnBookmarkBarVisibilityChanged,
                 base::Unretained(this)));

  top_sites_ = TopSitesFactory::GetForProfile(profile_);
  if (top_sites_) {
    GetTopSitesData();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    scoped_observer_.Add(top_sites_.get());
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  avatar_menu_ = std::make_unique<AvatarMenu>(
      &profile_manager->GetProfileAttributesStorage(), this,
      BrowserList::GetInstance()->GetLastActive());
  avatar_menu_->RebuildMenu();
  BrowserList::AddObserver(this);

  RebuildProfilesMenu();

  menu_service_ = std::make_unique<DbusMenu>(
      GlobalMenuBarRegistrarX11::GetInstance()->bus()->GetExportedObject(
          dbus::ObjectPath(GetPath())),
      std::move(callback));
  menu_service_->SetModel(root_menu_.get(), false);
}

std::string GlobalMenuBarX11::GetPath() const {
  return base::StringPrintf("/com/canonical/menu/%lX", xid_);
}

ui::SimpleMenuModel* GlobalMenuBarX11::BuildStaticMenu(
    int string_id,
    const GlobalMenuBarCommand* commands) {
  toplevel_menus_.push_back(std::make_unique<ui::SimpleMenuModel>(this));
  ui::SimpleMenuModel* menu = toplevel_menus_.back().get();
  for (; commands->command != kMenuEnd; commands++) {
    int command_id = commands->command;
    if (command_id == kSeparator) {
      // Use InsertSeparatorAt() instead of AddSeparator() because the latter
      // refuses to add a separator to an empty menu.
      int old_item_count = menu->GetItemCount();
      menu->InsertSeparatorAt(old_item_count,
                              ui::MenuSeparatorType::SPACING_SEPARATOR);

      // Make extra sure the separator got added in case the behavior
      // InsertSeparatorAt() changes.
      CHECK_EQ(old_item_count + 1, menu->GetItemCount());
      continue;
    }

    int string_id = commands->str_id;
    if (command_id == IDC_SHOW_BOOKMARK_BAR)
      menu->AddCheckItemWithStringId(command_id, string_id);
    else
      menu->AddItemWithStringId(command_id, string_id);
    if (command_id < kLastChromeCommand)
      RegisterCommandObserver(command_id);
  }
  root_menu_->AddSubMenu(kSubmenu, l10n_util::GetStringUTF16(string_id), menu);
  return menu;
}

std::unique_ptr<GlobalMenuBarX11::HistoryItem>
GlobalMenuBarX11::HistoryItemForTab(
    const sessions::TabRestoreService::Tab& entry) {
  const sessions::SerializedNavigationEntry& current_navigation =
      entry.navigations.at(entry.current_navigation_index);
  auto item = std::make_unique<HistoryItem>();
  item->title = current_navigation.title();
  item->url = current_navigation.virtual_url();
  item->session_id = entry.id;
  return item;
}

void GlobalMenuBarX11::AddHistoryItemToMenu(std::unique_ptr<HistoryItem> item,
                                            ui::SimpleMenuModel* menu,
                                            int index) {
  base::string16 title = item->title;
  std::string url_string = item->url.possibly_invalid_spec();

  if (title.empty())
    title = base::UTF8ToUTF16(url_string);
  gfx::ElideString(title, kMaximumMenuWidthInChars, &title);

  int command_id = NextCommandId();
  menu->InsertItemAt(index, command_id, title);
  history_items_[command_id] = std::move(item);
}

void GlobalMenuBarX11::GetTopSitesData() {
  DCHECK(top_sites_);

  top_sites_->GetMostVisitedURLs(base::Bind(
      &GlobalMenuBarX11::OnTopSitesReceived, weak_ptr_factory_.GetWeakPtr()));
}

void GlobalMenuBarX11::OnTopSitesReceived(
    const history::MostVisitedURLList& visited_list) {
  int index = ClearHistoryMenuSection(kTagMostVisited);

  for (size_t i = 0; i < visited_list.size() && i < kMostVisitedCount; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.spec().empty())
      break;  // This is the signal that there are no more real visited sites.

    auto item = std::make_unique<HistoryItem>();
    item->title = visited.title;
    item->url = visited.url;

    AddHistoryItemToMenu(std::move(item), history_menu_, index++);
  }

  if (menu_service_)
    menu_service_->MenuLayoutUpdated(history_menu_);
}

void GlobalMenuBarX11::OnBookmarkBarVisibilityChanged() {
  menu_service_->MenuItemsPropertiesUpdated(
      FindMenuItemsForCommand(root_menu_.get(), IDC_SHOW_BOOKMARK_BAR));
}

void GlobalMenuBarX11::RebuildProfilesMenu() {
  while (profiles_menu_->GetTypeAt(0) != ui::MenuModel::TYPE_SEPARATOR)
    profiles_menu_->RemoveItemAt(0);
  profile_commands_.clear();

  // Don't call avatar_menu_->GetActiveProfileIndex() as the as the index might
  // be incorrect if RebuildProfilesMenu() is called while we deleting the
  // active profile and closing all its browser windows.
  active_profile_index_ = -1;
  for (size_t i = 0; i < avatar_menu_->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(i);
    base::string16 title = item.name;
    gfx::ElideString(title, kMaximumMenuWidthInChars, &title);

    if (item.active)
      active_profile_index_ = i;

    int command = NextCommandId();
    profile_commands_[command] = i;
    profiles_menu_->InsertCheckItemAt(i, command, title);
  }

  if (menu_service_)
    menu_service_->MenuLayoutUpdated(profiles_menu_);
}

int GlobalMenuBarX11::ClearHistoryMenuSection(int header_command_id) {
  int index = 0;
  while (history_menu_->GetCommandIdAt(index++) != header_command_id) {
  }
  while (history_menu_->GetTypeAt(index) != ui::MenuModel::TYPE_SEPARATOR) {
    history_items_.erase(history_menu_->GetCommandIdAt(index));
    history_menu_->RemoveItemAt(index);
  }
  return index;
}

void GlobalMenuBarX11::RegisterCommandObserver(int command) {
  if (command > kLastChromeCommand)
    return;

  // Keep track of which commands are already registered to avoid
  // registering them twice.
  const bool inserted = observed_commands_.insert(command).second;
  if (!inserted)
    return;

  chrome::AddCommandObserver(browser_, command, this);
}

int GlobalMenuBarX11::NextCommandId() {
  do {
    if (last_command_id_ == std::numeric_limits<int>::max())
      last_command_id_ = kFirstUnreservedCommandId;
    else
      last_command_id_++;
  } while (base::Contains(history_items_, last_command_id_) ||
           base::Contains(profile_commands_, last_command_id_));
  return last_command_id_;
}

void GlobalMenuBarX11::OnAvatarMenuChanged(AvatarMenu* avatar_menu) {
  RebuildProfilesMenu();
}

void GlobalMenuBarX11::OnBrowserSetLastActive(Browser* browser) {
  // Notify the avatar menu of the change and rebuild the menu. Note: The
  // ActiveBrowserChanged() call needs to happen first to update the state.
  avatar_menu_->ActiveBrowserChanged(browser);
  avatar_menu_->RebuildMenu();
  RebuildProfilesMenu();
}

void GlobalMenuBarX11::EnabledStateChangedForCommand(int id, bool enabled) {
  menu_service_->MenuItemsPropertiesUpdated(
      FindMenuItemsForCommand(root_menu_.get(), id));
}

void GlobalMenuBarX11::TopSitesLoaded(history::TopSites* top_sites) {}

void GlobalMenuBarX11::TopSitesChanged(history::TopSites* top_sites,
                                       ChangeReason change_reason) {
  GetTopSitesData();
}

void GlobalMenuBarX11::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  const sessions::TabRestoreService::Entries& entries = service->entries();

  int index = ClearHistoryMenuSection(kTagRecentlyClosed);
  recently_closed_window_menus_.clear();

  unsigned int added_count = 0;
  for (auto it = entries.begin();
       it != entries.end() && added_count < kRecentlyClosedCount; ++it) {
    sessions::TabRestoreService::Entry* entry = it->get();

    if (entry->type == sessions::TabRestoreService::WINDOW) {
      sessions::TabRestoreService::Window* entry_win =
          static_cast<sessions::TabRestoreService::Window*>(entry);
      auto& tabs = entry_win->tabs;
      if (tabs.empty())
        continue;

      // Create the item for the parent/window.
      auto item = std::make_unique<HistoryItem>();
      item->session_id = entry_win->id;

      base::string16 title = l10n_util::GetPluralStringFUTF16(
          IDS_RECENTLY_CLOSED_WINDOW, tabs.size());

      auto parent_menu = std::make_unique<ui::SimpleMenuModel>(this);
      int command = NextCommandId();
      history_menu_->InsertSubMenuAt(index++, command, title,
                                     parent_menu.get());
      parent_menu->AddItemWithStringId(command,
                                       IDS_HISTORY_CLOSED_RESTORE_WINDOW_LINUX);
      parent_menu->AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);

      // Loop over the window's tabs and add them to the submenu.
      int subindex = 2;
      for (const auto& tab : tabs) {
        std::unique_ptr<HistoryItem> tab_item = HistoryItemForTab(*tab);
        item->tabs.push_back(tab_item.get());
        AddHistoryItemToMenu(std::move(tab_item), parent_menu.get(),
                             subindex++);
      }

      history_items_[command] = std::move(item);
      recently_closed_window_menus_.push_back(std::move(parent_menu));
      ++added_count;
    } else if (entry->type == sessions::TabRestoreService::TAB) {
      sessions::TabRestoreService::Tab* tab =
          static_cast<sessions::TabRestoreService::Tab*>(entry);
      AddHistoryItemToMenu(HistoryItemForTab(*tab), history_menu_, index++);
      ++added_count;
    }
  }

  menu_service_->MenuLayoutUpdated(history_menu_);
}

void GlobalMenuBarX11::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = nullptr;
}

bool GlobalMenuBarX11::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR) {
    return browser_->profile()->GetPrefs()->GetBoolean(
        bookmarks::prefs::kShowBookmarkBar);
  }

  auto it = profile_commands_.find(command_id);
  return it != profile_commands_.end() && it->second == active_profile_index_;
}

bool GlobalMenuBarX11::IsCommandIdEnabled(int command_id) const {
  if (command_id <= kLastChromeCommand)
    return chrome::IsCommandEnabled(browser_, command_id);
  // There is no active profile in Guest mode, in which case the action
  // buttons should be disabled.
  if (command_id == kTagProfileEdit || command_id == kTagProfileCreate)
    return active_profile_index_ >= 0;
  return command_id != kTagRecentlyClosed && command_id != kTagMostVisited;
}

void GlobalMenuBarX11::ExecuteCommand(int command_id, int event_flags) {
  if (command_id <= kLastChromeCommand) {
    chrome::ExecuteCommand(browser_, command_id);
  } else if (command_id == kTagProfileEdit) {
    avatar_menu_->EditProfile(active_profile_index_);
  } else if (command_id == kTagProfileCreate) {
    profiles::CreateAndSwitchToNewProfile(ProfileManager::CreateCallback(),
                                          ProfileMetrics::ADD_NEW_USER_MENU);
  } else if (base::Contains(history_items_, command_id)) {
    HistoryItem* item = history_items_[command_id].get();
    // If this item can be restored using TabRestoreService, do so.
    // Otherwise, just load the URL.
    sessions::TabRestoreService* service =
        TabRestoreServiceFactory::GetForProfile(profile_);
    if (item->session_id.is_valid() && service) {
      service->RestoreEntryById(browser_->live_tab_context(), item->session_id,
                                WindowOpenDisposition::UNKNOWN);
    } else {
      DCHECK(item->url.is_valid());
      browser_->OpenURL(
          content::OpenURLParams(item->url, content::Referrer(),
                                 WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                 ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    }
  } else if (base::Contains(profile_commands_, command_id)) {
    avatar_menu_->SwitchToProfile(profile_commands_[command_id], false,
                                  ProfileMetrics::SWITCH_PROFILE_MENU);
  }
}

void GlobalMenuBarX11::OnMenuWillShow(ui::SimpleMenuModel* source) {
  if (source != history_menu_ || tab_restore_service_)
    return;

  tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);
  if (!tab_restore_service_)
    return;

  tab_restore_service_->LoadTabsFromLastSession();
  tab_restore_service_->AddObserver(this);

  // If LoadTabsFromLastSession doesn't load tabs, it won't call
  // TabRestoreServiceChanged(). This ensures that all new windows after
  // the first one will have their menus populated correctly.
  TabRestoreServiceChanged(tab_restore_service_);
}

bool GlobalMenuBarX11::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return browser_view_->GetAccelerator(command_id, accelerator);
}
