// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/global_menu_bar_x11.h"

#include <dlfcn.h>
#include <glib-object.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_x11.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/global_menu_bar_registrar_x11.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/history/core/browser/top_sites.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/text_elider.h"

// libdbusmenu-glib types
typedef struct _DbusmenuMenuitem DbusmenuMenuitem;
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_new_func)();
typedef bool (*dbusmenu_menuitem_child_add_position_func)(
    DbusmenuMenuitem* parent,
    DbusmenuMenuitem* child,
    unsigned int position);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_child_append_func)(
    DbusmenuMenuitem* parent,
    DbusmenuMenuitem* child);
typedef bool (*dbusmenu_menuitem_child_delete_func)(
    DbusmenuMenuitem* parent,
    DbusmenuMenuitem* child);
typedef GList* (*dbusmenu_menuitem_get_children_func)(
    DbusmenuMenuitem* item);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_func)(
    DbusmenuMenuitem* item,
    const char* property,
    const char* value);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_variant_func)(
    DbusmenuMenuitem* item,
    const char* property,
    GVariant* value);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_bool_func)(
    DbusmenuMenuitem* item,
    const char* property,
    bool value);
typedef DbusmenuMenuitem* (*dbusmenu_menuitem_property_set_int_func)(
    DbusmenuMenuitem* item,
    const char* property,
    int value);

typedef struct _DbusmenuServer      DbusmenuServer;
typedef DbusmenuServer* (*dbusmenu_server_new_func)(const char* object);
typedef void (*dbusmenu_server_set_root_func)(DbusmenuServer* self,
                                              DbusmenuMenuitem* root);

// A line in the static menu definitions.
struct GlobalMenuBarCommand {
  int str_id;
  int command;
  int tag;
};

namespace {

// Retrieved functions from libdbusmenu-glib.

// DbusmenuMenuItem methods:
dbusmenu_menuitem_new_func menuitem_new = nullptr;
dbusmenu_menuitem_get_children_func menuitem_get_children = nullptr;
dbusmenu_menuitem_child_add_position_func menuitem_child_add_position = nullptr;
dbusmenu_menuitem_child_append_func menuitem_child_append = nullptr;
dbusmenu_menuitem_child_delete_func menuitem_child_delete = nullptr;
dbusmenu_menuitem_property_set_func menuitem_property_set = nullptr;
dbusmenu_menuitem_property_set_variant_func menuitem_property_set_variant =
    nullptr;
dbusmenu_menuitem_property_set_bool_func menuitem_property_set_bool = nullptr;
dbusmenu_menuitem_property_set_int_func menuitem_property_set_int = nullptr;

// DbusmenuServer methods:
dbusmenu_server_new_func server_new = nullptr;
dbusmenu_server_set_root_func server_set_root = nullptr;

// Properties that we set on menu items:
const char kPropertyEnabled[] = "enabled";
const char kPropertyLabel[] = "label";
const char kPropertyShortcut[] = "shortcut";
const char kPropertyType[] = "type";
const char kPropertyToggleType[] = "toggle-type";
const char kPropertyToggleState[] = "toggle-state";
const char kPropertyVisible[] = "visible";

const char kTypeCheckmark[] = "checkmark";
const char kTypeSeparator[] = "separator";

// Data set on GObjectgs.
const char kTypeTag[] = "type-tag";
const char kHistoryItem[] = "history-item";
const char kProfileId[] = "profile-id";

// The maximum number of most visited items to display.
const unsigned int kMostVisitedCount = 8;

// The number of recently closed items to get.
const unsigned int kRecentlyClosedCount = 8;

// Menus more than this many chars long will get trimmed.
const size_t kMaximumMenuWidthInChars = 50;

// Constants used in menu definitions.
const int MENU_SEPARATOR =-1;
const int MENU_END = -2;
const int MENU_DISABLED_ID = -3;

// These tag values are used to refer to menu items.
const int TAG_MOST_VISITED = 1;
const int TAG_RECENTLY_CLOSED = 2;
const int TAG_MOST_VISITED_HEADER = 3;
const int TAG_RECENTLY_CLOSED_HEADER = 4;
const int TAG_PROFILES = 5;

GlobalMenuBarCommand file_menu[] = {
  { IDS_NEW_TAB, IDC_NEW_TAB },
  { IDS_NEW_WINDOW, IDC_NEW_WINDOW },
  { IDS_NEW_INCOGNITO_WINDOW, IDC_NEW_INCOGNITO_WINDOW },
  { IDS_REOPEN_CLOSED_TABS_LINUX, IDC_RESTORE_TAB },
  { IDS_OPEN_FILE_LINUX, IDC_OPEN_FILE },
  { IDS_OPEN_LOCATION_LINUX, IDC_FOCUS_LOCATION },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_CLOSE_WINDOW_LINUX, IDC_CLOSE_WINDOW },
  { IDS_CLOSE_TAB_LINUX, IDC_CLOSE_TAB },
  { IDS_SAVE_PAGE, IDC_SAVE_PAGE },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_PRINT, IDC_PRINT },

  { MENU_END, MENU_END }
};

GlobalMenuBarCommand edit_menu[] = {
  { IDS_CUT, IDC_CUT },
  { IDS_COPY, IDC_COPY },
  { IDS_PASTE, IDC_PASTE },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_FIND, IDC_FIND },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_PREFERENCES, IDC_OPTIONS },

  { MENU_END, MENU_END }
};

GlobalMenuBarCommand view_menu[] = {
  { IDS_SHOW_BOOKMARK_BAR, IDC_SHOW_BOOKMARK_BAR },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_STOP_MENU_LINUX, IDC_STOP },
  { IDS_RELOAD_MENU_LINUX, IDC_RELOAD },

  { MENU_SEPARATOR, MENU_SEPARATOR },

  { IDS_FULLSCREEN, IDC_FULLSCREEN },
  { IDS_TEXT_DEFAULT_LINUX, IDC_ZOOM_NORMAL },
  { IDS_TEXT_BIGGER_LINUX, IDC_ZOOM_PLUS },
  { IDS_TEXT_SMALLER_LINUX, IDC_ZOOM_MINUS },

  { MENU_END, MENU_END }
};

GlobalMenuBarCommand history_menu[] = {
    {IDS_HISTORY_HOME_LINUX, IDC_HOME},
    {IDS_HISTORY_BACK_LINUX, IDC_BACK},
    {IDS_HISTORY_FORWARD_LINUX, IDC_FORWARD},

    {MENU_SEPARATOR, MENU_SEPARATOR},

    {IDS_HISTORY_CLOSED_LINUX, MENU_DISABLED_ID, TAG_RECENTLY_CLOSED_HEADER},

    {MENU_SEPARATOR, MENU_SEPARATOR},

    {IDS_HISTORY_VISITED_LINUX, MENU_DISABLED_ID, TAG_MOST_VISITED_HEADER},

    {MENU_SEPARATOR, MENU_SEPARATOR},

    {IDS_HISTORY_SHOWFULLHISTORY_LINK, IDC_SHOW_HISTORY},

    {MENU_END, MENU_END}};

GlobalMenuBarCommand tools_menu[] = {
    {IDS_SHOW_DOWNLOADS, IDC_SHOW_DOWNLOADS},
    {IDS_HISTORY_SHOW_HISTORY, IDC_SHOW_HISTORY},
    {IDS_SHOW_EXTENSIONS, IDC_MANAGE_EXTENSIONS},

    {MENU_SEPARATOR, MENU_SEPARATOR},

    {IDS_TASK_MANAGER, IDC_TASK_MANAGER},
    {IDS_CLEAR_BROWSING_DATA, IDC_CLEAR_BROWSING_DATA},

    {MENU_SEPARATOR, MENU_SEPARATOR},

    {IDS_VIEW_SOURCE, IDC_VIEW_SOURCE},
    {IDS_DEV_TOOLS, IDC_DEV_TOOLS},
    {IDS_DEV_TOOLS_CONSOLE, IDC_DEV_TOOLS_CONSOLE},
    {IDS_DEV_TOOLS_DEVICES, IDC_DEV_TOOLS_DEVICES},

    {MENU_END, MENU_END}};

GlobalMenuBarCommand help_menu[] = {
#if defined(GOOGLE_CHROME_BUILD)
  { IDS_FEEDBACK, IDC_FEEDBACK },
#endif
  { IDS_HELP_PAGE , IDC_HELP_PAGE_VIA_MENU },
  { MENU_END, MENU_END }
};

GlobalMenuBarCommand profiles_menu[] = {
  { MENU_SEPARATOR, MENU_SEPARATOR },
  { MENU_END, MENU_END }
};

void EnsureMethodsLoaded() {
  static bool attempted_load = false;
  if (attempted_load)
    return;
  attempted_load = true;

  void* dbusmenu_lib = dlopen("libdbusmenu-glib.so", RTLD_LAZY);
  if (!dbusmenu_lib)
    dbusmenu_lib = dlopen("libdbusmenu-glib.so.4", RTLD_LAZY);
  if (!dbusmenu_lib)
    return;

  // DbusmenuMenuItem methods.
  menuitem_new = reinterpret_cast<dbusmenu_menuitem_new_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_new"));
  menuitem_child_add_position =
      reinterpret_cast<dbusmenu_menuitem_child_add_position_func>(
          dlsym(dbusmenu_lib, "dbusmenu_menuitem_child_add_position"));
  menuitem_child_append = reinterpret_cast<dbusmenu_menuitem_child_append_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_child_append"));
  menuitem_child_delete = reinterpret_cast<dbusmenu_menuitem_child_delete_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_child_delete"));
  menuitem_get_children = reinterpret_cast<dbusmenu_menuitem_get_children_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_get_children"));
  menuitem_property_set = reinterpret_cast<dbusmenu_menuitem_property_set_func>(
      dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set"));
  menuitem_property_set_variant =
      reinterpret_cast<dbusmenu_menuitem_property_set_variant_func>(
          dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set_variant"));
  menuitem_property_set_bool =
      reinterpret_cast<dbusmenu_menuitem_property_set_bool_func>(
          dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set_bool"));
  menuitem_property_set_int =
      reinterpret_cast<dbusmenu_menuitem_property_set_int_func>(
          dlsym(dbusmenu_lib, "dbusmenu_menuitem_property_set_int"));

  // DbusmenuServer methods.
  server_new = reinterpret_cast<dbusmenu_server_new_func>(
      dlsym(dbusmenu_lib, "dbusmenu_server_new"));
  server_set_root = reinterpret_cast<dbusmenu_server_set_root_func>(
      dlsym(dbusmenu_lib, "dbusmenu_server_set_root"));
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
  // that this is a list of weak references. The |menu_item_map_| is the owner
  // of all items. If it is not a window, then the entry is a single page and
  // the vector will be empty.
  std::vector<HistoryItem*> tabs;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryItem);
};

GlobalMenuBarX11::GlobalMenuBarX11(BrowserView* browser_view,
                                   BrowserDesktopWindowTreeHostX11* host)
    : browser_(browser_view->browser()),
      profile_(browser_->profile()),
      browser_view_(browser_view),
      host_(host),
      server_(nullptr),
      root_item_(nullptr),
      history_menu_(nullptr),
      profiles_menu_(nullptr),
      top_sites_(nullptr),
      tab_restore_service_(nullptr),
      scoped_observer_(this),
      weak_ptr_factory_(this) {
  EnsureMethodsLoaded();

  if (server_new)
    host_->AddObserver(this);
}

GlobalMenuBarX11::~GlobalMenuBarX11() {
  if (server_) {
    Disable();

    if (tab_restore_service_)
      tab_restore_service_->RemoveObserver(this);

    g_object_unref(server_);
    host_->RemoveObserver(this);
  }
  BrowserList::RemoveObserver(this);
}

// static
std::string GlobalMenuBarX11::GetPathForWindow(XID xid) {
  return base::StringPrintf("/com/canonical/menu/%lX", xid);
}

DbusmenuMenuitem* GlobalMenuBarX11::BuildSeparator() {
  DbusmenuMenuitem* item = menuitem_new();
  menuitem_property_set(item, kPropertyType, kTypeSeparator);
  menuitem_property_set_bool(item, kPropertyVisible, true);
  return item;
}

DbusmenuMenuitem* GlobalMenuBarX11::BuildMenuItem(
    const std::string& label,
    int tag_id) {
  DbusmenuMenuitem* item = menuitem_new();
  menuitem_property_set(item, kPropertyLabel, label.c_str());
  menuitem_property_set_bool(item, kPropertyVisible, true);

  if (tag_id)
    g_object_set_data(G_OBJECT(item), kTypeTag, GINT_TO_POINTER(tag_id));

  return item;
}

void GlobalMenuBarX11::InitServer(XID xid) {
  std::string path = GetPathForWindow(xid);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;  // http://crbug.com/314087
    server_ = server_new(path.c_str());
  }

  root_item_ = menuitem_new();
  menuitem_property_set(root_item_, kPropertyLabel, "Root");
  menuitem_property_set_bool(root_item_, kPropertyVisible, true);

  // First build static menu content.
  BuildStaticMenu(root_item_, IDS_FILE_MENU_LINUX, file_menu);
  BuildStaticMenu(root_item_, IDS_EDIT_MENU_LINUX, edit_menu);
  BuildStaticMenu(root_item_, IDS_VIEW_MENU_LINUX, view_menu);
  history_menu_ = BuildStaticMenu(
      root_item_, IDS_HISTORY_MENU_LINUX, history_menu);
  BuildStaticMenu(root_item_, IDS_TOOLS_MENU_LINUX, tools_menu);
  profiles_menu_ = BuildStaticMenu(
      root_item_, IDS_PROFILES_OPTIONS_GROUP_NAME, profiles_menu);
  BuildStaticMenu(root_item_, IDS_HELP_MENU_LINUX, help_menu);

  // We have to connect to |history_menu_item|'s "activate" signal instead of
  // |history_menu|'s "show" signal because we are not supposed to modify the
  // menu during "show"
  g_signal_connect(history_menu_, "about-to-show",
                   G_CALLBACK(OnHistoryMenuAboutToShowThunk), this);

  for (CommandIDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    menuitem_property_set_bool(it->second, kPropertyEnabled,
                               chrome::IsCommandEnabled(browser_, it->first));

    ui::Accelerator accelerator;
    if (browser_view_->GetAccelerator(it->first, &accelerator))
      RegisterAccelerator(it->second, accelerator);

    chrome::AddCommandObserver(browser_, it->first, this);
  }

  pref_change_registrar_.Init(browser_->profile()->GetPrefs());
  pref_change_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::Bind(&GlobalMenuBarX11::OnBookmarkBarVisibilityChanged,
                 base::Unretained(this)));
  OnBookmarkBarVisibilityChanged();

  top_sites_ = TopSitesFactory::GetForProfile(profile_);
  if (top_sites_) {
    GetTopSitesData();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    scoped_observer_.Add(top_sites_.get());
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  avatar_menu_.reset(new AvatarMenu(
      &profile_manager->GetProfileAttributesStorage(), this, nullptr));
  avatar_menu_->RebuildMenu();
  BrowserList::AddObserver(this);

  RebuildProfilesMenu();

  server_set_root(server_, root_item_);
}

void GlobalMenuBarX11::Disable() {
  for (CommandIDMenuItemMap::const_iterator it = id_to_menu_item_.begin();
       it != id_to_menu_item_.end(); ++it) {
    chrome::RemoveCommandObserver(browser_, it->first, this);
  }
  id_to_menu_item_.clear();

  pref_change_registrar_.RemoveAll();
}

DbusmenuMenuitem* GlobalMenuBarX11::BuildStaticMenu(
    DbusmenuMenuitem* parent,
    int menu_str_id,
    GlobalMenuBarCommand* commands) {
  DbusmenuMenuitem* top = menuitem_new();
  menuitem_property_set(
      top, kPropertyLabel,
      ui::RemoveWindowsStyleAccelerators(l10n_util::GetStringUTF8(menu_str_id))
          .c_str());
  menuitem_property_set_bool(top, kPropertyVisible, true);

  for (int i = 0; commands[i].str_id != MENU_END; ++i) {
    DbusmenuMenuitem* menu_item = nullptr;
    int command_id = commands[i].command;
    if (commands[i].str_id == MENU_SEPARATOR) {
      menu_item = BuildSeparator();
    } else {
      std::string label = ui::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(commands[i].str_id));

      menu_item = BuildMenuItem(label, commands[i].tag);

      if (command_id == MENU_DISABLED_ID) {
        menuitem_property_set_bool(menu_item, kPropertyEnabled, false);
      } else {
        if (command_id == IDC_SHOW_BOOKMARK_BAR)
          menuitem_property_set(menu_item, kPropertyToggleType, kTypeCheckmark);

        id_to_menu_item_.insert(std::make_pair(command_id, menu_item));
        g_object_set_data(G_OBJECT(menu_item), "command-id",
                          GINT_TO_POINTER(command_id));
        g_signal_connect(menu_item, "item-activated",
                         G_CALLBACK(OnItemActivatedThunk), this);
      }
    }

    menuitem_child_append(top, menu_item);
    g_object_unref(menu_item);
  }

  menuitem_child_append(parent, top);
  g_object_unref(top);
  return top;
}

void GlobalMenuBarX11::RegisterAccelerator(DbusmenuMenuitem* item,
                                           const ui::Accelerator& accelerator) {
  // A translation of libdbusmenu-gtk's menuitem_property_set_shortcut()
  // translated from GDK types to ui::Accelerator types.
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);

  if (accelerator.IsCtrlDown())
    g_variant_builder_add(&builder, "s", "Control");
  if (accelerator.IsAltDown())
    g_variant_builder_add(&builder, "s", "Alt");
  if (accelerator.IsShiftDown())
    g_variant_builder_add(&builder, "s", "Shift");

  char* name = XKeysymToString(XKeysymForWindowsKeyCode(
      accelerator.key_code(), false));
  if (!name) {
    NOTIMPLEMENTED();
    return;
  }
  g_variant_builder_add(&builder, "s", name);

  GVariant* inside_array = g_variant_builder_end(&builder);
  g_variant_builder_init(&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add_value(&builder, inside_array);
  GVariant* outside_array = g_variant_builder_end(&builder);

  menuitem_property_set_variant(item, kPropertyShortcut, outside_array);
}

GlobalMenuBarX11::HistoryItem* GlobalMenuBarX11::HistoryItemForTab(
    const sessions::TabRestoreService::Tab& entry) {
  const sessions::SerializedNavigationEntry& current_navigation =
      entry.navigations.at(entry.current_navigation_index);
  HistoryItem* item = new HistoryItem();
  item->title = current_navigation.title();
  item->url = current_navigation.virtual_url();
  item->session_id = entry.id;

  return item;
}

void GlobalMenuBarX11::AddHistoryItemToMenu(HistoryItem* item,
                                            DbusmenuMenuitem* menu,
                                            int tag,
                                            int index) {
  base::string16 title = item->title;
  std::string url_string = item->url.possibly_invalid_spec();

  if (title.empty())
    title = base::UTF8ToUTF16(url_string);
  gfx::ElideString(title, kMaximumMenuWidthInChars, &title);

  DbusmenuMenuitem* menu_item = BuildMenuItem(base::UTF16ToUTF8(title), tag);
  g_signal_connect(menu_item, "item-activated",
                   G_CALLBACK(OnHistoryItemActivatedThunk), this);

  g_object_set_data_full(G_OBJECT(menu_item), kHistoryItem, item,
                         DeleteHistoryItem);
  menuitem_child_add_position(menu, menu_item, index);
  g_object_unref(menu_item);
}

void GlobalMenuBarX11::GetTopSitesData() {
  DCHECK(top_sites_);

  top_sites_->GetMostVisitedURLs(
      base::Bind(&GlobalMenuBarX11::OnTopSitesReceived,
                 weak_ptr_factory_.GetWeakPtr()), false);
}

void GlobalMenuBarX11::OnTopSitesReceived(
    const history::MostVisitedURLList& visited_list) {
  ClearMenuSection(history_menu_, TAG_MOST_VISITED);

  int index = GetIndexOfMenuItemWithTag(history_menu_,
                                        TAG_MOST_VISITED_HEADER) + 1;

  for (size_t i = 0; i < visited_list.size() && i < kMostVisitedCount; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.spec().empty())
      break;  // This is the signal that there are no more real visited sites.

    HistoryItem* item = new HistoryItem();
    item->title = visited.title;
    item->url = visited.url;

    AddHistoryItemToMenu(item,
                         history_menu_,
                         TAG_MOST_VISITED,
                         index++);
  }
}

void GlobalMenuBarX11::OnBookmarkBarVisibilityChanged() {
  auto it = id_to_menu_item_.find(IDC_SHOW_BOOKMARK_BAR);
  if (it != id_to_menu_item_.end()) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    // Note: Unlike the GTK version, we don't appear to need to do tricks where
    // we block activation while setting the toggle.
    menuitem_property_set_int(
        it->second,
        kPropertyToggleState,
        prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  }
}

void GlobalMenuBarX11::RebuildProfilesMenu() {
  ClearMenuSection(profiles_menu_, TAG_PROFILES);

  // Don't call avatar_menu_->GetActiveProfileIndex() as the as the index might
  // be incorrect if RebuildProfilesMenu() is called while we deleting the
  // active profile and closing all its browser windows.
  int active_profile_index = -1;

  for (size_t i = 0; i < avatar_menu_->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(i);
    base::string16 title = item.name;
    gfx::ElideString(title, kMaximumMenuWidthInChars, &title);

    DbusmenuMenuitem* menu_item = BuildMenuItem(
        base::UTF16ToUTF8(title), TAG_PROFILES);
    g_object_set_data(G_OBJECT(menu_item), kProfileId, GINT_TO_POINTER(i));
    g_signal_connect(menu_item, "item-activated",
                     G_CALLBACK(OnProfileItemActivatedThunk), this);
    menuitem_property_set(menu_item, kPropertyToggleType, kTypeCheckmark);
    menuitem_property_set_int(menu_item, kPropertyToggleState, item.active);

    if (item.active)
      active_profile_index = i;

    menuitem_child_add_position(profiles_menu_, menu_item, i);
    g_object_unref(menu_item);
  }

  // There is a separator between the list of profiles and the possible actions.
  int index = avatar_menu_->GetNumberOfItems() + 1;

  DbusmenuMenuitem* edit_profile_item = BuildMenuItem(
      l10n_util::GetStringUTF8(IDS_PROFILES_MANAGE_BUTTON_LABEL), TAG_PROFILES);
  DbusmenuMenuitem* create_profile_item = BuildMenuItem(
      l10n_util::GetStringUTF8(IDS_PROFILES_CREATE_BUTTON_LABEL),
      TAG_PROFILES);

  // There is no active profile in Guest mode, in which case the action buttons
  // should be disabled.
  if (active_profile_index >= 0) {
    g_object_set_data(G_OBJECT(edit_profile_item), kProfileId,
                      GINT_TO_POINTER(active_profile_index));
    g_signal_connect(edit_profile_item, "item-activated",
                     G_CALLBACK(OnEditProfileItemActivatedThunk), this);
    g_signal_connect(create_profile_item, "item-activated",
                     G_CALLBACK(OnCreateProfileItemActivatedThunk), this);
  } else {
    menuitem_property_set_bool(edit_profile_item, kPropertyEnabled, false);
    menuitem_property_set_bool(create_profile_item, kPropertyEnabled, false);
  }

  menuitem_child_add_position(profiles_menu_, edit_profile_item, index++);
  menuitem_child_add_position(profiles_menu_, create_profile_item, index);
  g_object_unref(edit_profile_item);
  g_object_unref(create_profile_item);
}

int GlobalMenuBarX11::GetIndexOfMenuItemWithTag(DbusmenuMenuitem* menu,
                                                int tag_id) {
  GList* childs = menuitem_get_children(menu);
  int i = 0;
  for (; childs != nullptr; childs = childs->next, i++) {
    int tag =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(childs->data), kTypeTag));
    if (tag == tag_id)
      return i;
  }

  NOTREACHED();
  return -1;
}

void GlobalMenuBarX11::ClearMenuSection(DbusmenuMenuitem* menu, int tag_id) {
  std::vector<DbusmenuMenuitem*> menuitems_to_delete;

  GList* childs = menuitem_get_children(menu);
  for (; childs != nullptr; childs = childs->next) {
    DbusmenuMenuitem* current_item = reinterpret_cast<DbusmenuMenuitem*>(
        childs->data);
    ClearMenuSection(current_item, tag_id);

    int tag =
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(childs->data), kTypeTag));
    if (tag == tag_id)
      menuitems_to_delete.push_back(current_item);
  }

  for (std::vector<DbusmenuMenuitem*>::const_iterator it =
           menuitems_to_delete.begin(); it != menuitems_to_delete.end(); ++it) {
    menuitem_child_delete(menu, *it);
  }
}

// static
void GlobalMenuBarX11::DeleteHistoryItem(void* void_item) {
  HistoryItem* item =
      reinterpret_cast<GlobalMenuBarX11::HistoryItem*>(void_item);
  delete item;
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
  auto it = id_to_menu_item_.find(id);
  if (it != id_to_menu_item_.end())
    menuitem_property_set_bool(it->second, kPropertyEnabled, enabled);
}

void GlobalMenuBarX11::TopSitesLoaded(history::TopSites* top_sites) {
}

void GlobalMenuBarX11::TopSitesChanged(history::TopSites* top_sites,
                                       ChangeReason change_reason) {
    GetTopSitesData();
}

void GlobalMenuBarX11::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  const sessions::TabRestoreService::Entries& entries = service->entries();

  ClearMenuSection(history_menu_, TAG_RECENTLY_CLOSED);

  // We'll get the index the "Recently Closed" header. (This can vary depending
  // on the number of "Most Visited" items.
  int index = GetIndexOfMenuItemWithTag(history_menu_,
                                        TAG_RECENTLY_CLOSED_HEADER) + 1;

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
      HistoryItem* item = new HistoryItem();
      item->session_id = entry_win->id;

      std::string title = l10n_util::GetPluralStringFUTF8(
          IDS_RECENTLY_CLOSED_WINDOW, tabs.size());
      DbusmenuMenuitem* parent_item = BuildMenuItem(
          title, TAG_RECENTLY_CLOSED);
      menuitem_child_add_position(history_menu_, parent_item, index++);
      g_object_unref(parent_item);

      // The mac version of this code allows the user to click on the parent
      // menu item to have the same effect as clicking the restore window
      // submenu item. GTK+ helpfully activates a menu item when it shows a
      // submenu so toss that feature out.
      DbusmenuMenuitem* restore_item = BuildMenuItem(
          l10n_util::GetStringUTF8(
              IDS_HISTORY_CLOSED_RESTORE_WINDOW_LINUX).c_str(),
          TAG_RECENTLY_CLOSED);
      g_signal_connect(restore_item, "item-activated",
                       G_CALLBACK(OnHistoryItemActivatedThunk), this);
      g_object_set_data_full(G_OBJECT(restore_item), kHistoryItem, item,
                             DeleteHistoryItem);
      menuitem_child_append(parent_item, restore_item);
      g_object_unref(restore_item);

      DbusmenuMenuitem* separator = BuildSeparator();
      menuitem_child_append(parent_item, separator);
      g_object_unref(separator);

      // Loop over the window's tabs and add them to the submenu.
      int subindex = 2;
      for (const auto& tab : tabs) {
        HistoryItem* tab_item = HistoryItemForTab(*tab);
        item->tabs.push_back(tab_item);
        AddHistoryItemToMenu(tab_item,
                             parent_item,
                             TAG_RECENTLY_CLOSED,
                             subindex++);
      }

      ++added_count;
    } else if (entry->type == sessions::TabRestoreService::TAB) {
      sessions::TabRestoreService::Tab* tab =
          static_cast<sessions::TabRestoreService::Tab*>(entry);
      HistoryItem* item = HistoryItemForTab(*tab);
      AddHistoryItemToMenu(item,
                           history_menu_,
                           TAG_RECENTLY_CLOSED,
                           index++);
      ++added_count;
    }
  }
}

void GlobalMenuBarX11::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = nullptr;
}

void GlobalMenuBarX11::OnWindowMapped(XID xid) {
  if (!server_)
    InitServer(xid);

  GlobalMenuBarRegistrarX11::GetInstance()->OnWindowMapped(xid);
}

void GlobalMenuBarX11::OnWindowUnmapped(XID xid) {
  GlobalMenuBarRegistrarX11::GetInstance()->OnWindowUnmapped(xid);
}

void GlobalMenuBarX11::OnItemActivated(DbusmenuMenuitem* item,
                                       unsigned int timestamp) {
  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "command-id"));
  chrome::ExecuteCommand(browser_, id);
}

void GlobalMenuBarX11::OnHistoryItemActivated(DbusmenuMenuitem* sender,
                                              unsigned int timestamp) {
  // Note: We don't have access to the event modifiers used to click the menu
  // item since that happens in a different process.
  HistoryItem* item = reinterpret_cast<HistoryItem*>(
      g_object_get_data(G_OBJECT(sender), kHistoryItem));

  // If this item can be restored using TabRestoreService, do so. Otherwise,
  // just load the URL.
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
}

void GlobalMenuBarX11::OnHistoryMenuAboutToShow(DbusmenuMenuitem* item) {
  if (!tab_restore_service_) {
    tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_restore_service_) {
      tab_restore_service_->LoadTabsFromLastSession();
      tab_restore_service_->AddObserver(this);

      // If LoadTabsFromLastSession doesn't load tabs, it won't call
      // TabRestoreServiceChanged(). This ensures that all new windows after
      // the first one will have their menus populated correctly.
      TabRestoreServiceChanged(tab_restore_service_);
    }
  }
}

void GlobalMenuBarX11::OnProfileItemActivated(DbusmenuMenuitem* sender,
                                              unsigned int timestamp) {
  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sender), kProfileId));
  avatar_menu_->SwitchToProfile(id, false, ProfileMetrics::SWITCH_PROFILE_MENU);
}

void GlobalMenuBarX11::OnEditProfileItemActivated(DbusmenuMenuitem* sender,
                                                  unsigned int timestamp) {
  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sender), kProfileId));
  avatar_menu_->EditProfile(id);
}

void GlobalMenuBarX11::OnCreateProfileItemActivated(DbusmenuMenuitem* sender,
                                                    unsigned int timestamp) {
  profiles::CreateAndSwitchToNewProfile(ProfileManager::CreateCallback(),
                                        ProfileMetrics::ADD_NEW_USER_MENU);
}
