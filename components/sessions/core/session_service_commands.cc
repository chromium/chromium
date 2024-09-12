// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_service_commands.h"

#include <stdint.h>
#include <string.h>

#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/pickle.h"
#include "base/token.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/sessions/core/base_session_service_commands.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace sessions {

// Identifier for commands written to file.
static const SessionCommand::id_type kCommandSetTabWindow = 0;
// OBSOLETE Superseded by kCommandSetWindowBounds3.
// static const SessionCommand::id_type kCommandSetWindowBounds = 1;
static const SessionCommand::id_type kCommandSetTabIndexInWindow = 2;

// OBSOLETE: Preserved for backward compatibility. Using
// kCommandTabNavigationPathPruned instead
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromBack = 5;

static const SessionCommand::id_type kCommandUpdateTabNavigation = 6;
static const SessionCommand::id_type kCommandSetSelectedNavigationIndex = 7;
static const SessionCommand::id_type kCommandSetSelectedTabInIndex = 8;
static const SessionCommand::id_type kCommandSetWindowType = 9;
// OBSOLETE Superseded by kCommandSetWindowBounds3. Except for data migration.
// static const SessionCommand::id_type kCommandSetWindowBounds2 = 10;

// OBSOLETE: Preserved for backward compatibility. Using
// kCommandTabNavigationPathPruned instead
static const SessionCommand::id_type
    kCommandTabNavigationPathPrunedFromFront = 11;

static const SessionCommand::id_type kCommandSetPinnedState = 12;
static const SessionCommand::id_type kCommandSetExtensionAppID = 13;
static const SessionCommand::id_type kCommandSetWindowBounds3 = 14;
static const SessionCommand::id_type kCommandSetWindowAppName = 15;
static const SessionCommand::id_type kCommandTabClosed = 16;
static const SessionCommand::id_type kCommandWindowClosed = 17;
// OBSOLETE: Superseded by kCommandSetTabUserAgentOverride2.
static const SessionCommand::id_type kCommandSetTabUserAgentOverride = 18;
static const SessionCommand::id_type kCommandSessionStorageAssociated = 19;
static const SessionCommand::id_type kCommandSetActiveWindow = 20;
static const SessionCommand::id_type kCommandLastActiveTime = 21;
// OBSOLETE Superseded by kCommandSetWindowWorkspace2.
// static const SessionCommand::id_type kCommandSetWindowWorkspace = 22;
static const SessionCommand::id_type kCommandSetWindowWorkspace2 = 23;
static const SessionCommand::id_type kCommandTabNavigationPathPruned = 24;
static const SessionCommand::id_type kCommandSetTabGroup = 25;
// OBSOLETE Superseded by kCommandSetTabGroupMetadata2.
// static const SessionCommand::id_type kCommandSetTabGroupMetadata = 26;
static const SessionCommand::id_type kCommandSetTabGroupMetadata2 = 27;
static const SessionCommand::id_type kCommandSetTabGuid = 28;
static const SessionCommand::id_type kCommandSetTabUserAgentOverride2 = 29;
static const SessionCommand::id_type kCommandSetTabData = 30;
static const SessionCommand::id_type kCommandSetWindowUserTitle = 31;
static const SessionCommand::id_type kCommandSetWindowVisibleOnAllWorkspaces =
    32;
static const SessionCommand::id_type kCommandAddTabExtraData = 33;
static const SessionCommand::id_type kCommandAddWindowExtraData = 34;
// ID 255 is used by CommandStorageBackend.

namespace {

// Various payload structures.
struct ClosedPayload {
  SessionID::id_type id;
  int64_t close_time;
};

struct WindowBoundsPayload2 {
  SessionID::id_type window_id;
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  bool is_maximized;
};

struct WindowBoundsPayload3 {
  SessionID::id_type window_id;
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  int32_t show_state;
};

using ActiveWindowPayload = SessionID::id_type;

struct IDAndIndexPayload {
  SessionID::id_type id;
  int32_t index;
};

using TabIndexInWindowPayload = IDAndIndexPayload;

using TabNavigationPathPrunedFromBackPayload = IDAndIndexPayload;

using SelectedNavigationIndexPayload = IDAndIndexPayload;

using SelectedTabInIndexPayload = IDAndIndexPayload;

using WindowTypePayload = IDAndIndexPayload;

using TabNavigationPathPrunedFromFrontPayload = IDAndIndexPayload;

struct TabNavigationPathPrunedPayload {
  SessionID::id_type id;
  // Index starting which |count| entries were removed.
  int32_t index;
  // Number of entries removed.
  int32_t count;
};

struct SerializedToken {
  // These fields correspond to the high and low fields of |base::Token|.
  uint64_t id_high;
  uint64_t id_low;
};

struct TabGroupPayload {
  SessionID::id_type tab_id;
  SerializedToken maybe_group;
  bool has_group;
};

struct PinnedStatePayload {
  SessionID::id_type tab_id;
  bool pinned_state;
};

struct LastActiveTimePayload {
  SessionID::id_type tab_id;
  int64_t last_active_time;
};

struct VisibleOnAllWorkspacesPayload {
  SessionID::id_type window_id;
  bool visible_on_all_workspaces;
};

// Persisted versions of ui::mojom::WindowShowState that are written to disk and
// can never change.
enum PersistedWindowShowState {
  // SHOW_STATE_DEFAULT (0) never persisted.
  PERSISTED_SHOW_STATE_NORMAL = 1,
  PERSISTED_SHOW_STATE_MINIMIZED = 2,
  PERSISTED_SHOW_STATE_MAXIMIZED = 3,
  // SHOW_STATE_INACTIVE (4) never persisted.
  PERSISTED_SHOW_STATE_FULLSCREEN = 5,
  PERSISTED_SHOW_STATE_DETACHED_DEPRECATED = 6,
  PERSISTED_SHOW_STATE_DOCKED_DEPRECATED = 7,
  PERSISTED_SHOW_STATE_END = 8,
};

// TODO(crbug.com/40946710): Remove this around December 2024. This is part of a
// workaround added to support the transition from storing the last_active_time
// as TimeTicks to Time that was added in December 2023. This is the threshold
// at which we consider that if a tab is so far in the past, it must be a tab
// serialized with TimeTicks and not Time.
const base::TimeDelta kLastActiveWorkaroundThreshold = base::Days(366 * 15);

// Assert to ensure PersistedWindowShowState is updated if ui::WindowShowState
// is changed.
// TODO(crbug.com/361560784): Investigate and Remove `kEnd`
static_assert(
    ui::mojom::WindowShowState::kEnd ==
        static_cast<ui::mojom::WindowShowState>(PERSISTED_SHOW_STATE_END - 2),
    "WindowShowState::kEnd must equal PERSISTED_SHOW_STATE_END minus the "
    "deprecated entries");
// Returns the show state to store to disk based |state|.
PersistedWindowShowState ShowStateToPersistedShowState(
    ui::mojom::WindowShowState state) {
  switch (state) {
    case ui::mojom::WindowShowState::kNormal:
      return PERSISTED_SHOW_STATE_NORMAL;
    case ui::mojom::WindowShowState::kMinimized:
      return PERSISTED_SHOW_STATE_MINIMIZED;
    case ui::mojom::WindowShowState::kMaximized:
      return PERSISTED_SHOW_STATE_MAXIMIZED;
    case ui::mojom::WindowShowState::kFullscreen:
      return PERSISTED_SHOW_STATE_FULLSCREEN;
    case ui::mojom::WindowShowState::kDefault:
    case ui::mojom::WindowShowState::kInactive:
      return PERSISTED_SHOW_STATE_NORMAL;

    case ui::mojom::WindowShowState::kEnd:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return PERSISTED_SHOW_STATE_NORMAL;
}

// Lints show state values when read back from persited disk.
ui::mojom::WindowShowState PersistedShowStateToShowState(int state) {
  switch (state) {
    case PERSISTED_SHOW_STATE_NORMAL:
      return ui::mojom::WindowShowState::kNormal;
    case PERSISTED_SHOW_STATE_MINIMIZED:
      return ui::mojom::WindowShowState::kMinimized;
    case PERSISTED_SHOW_STATE_MAXIMIZED:
      return ui::mojom::WindowShowState::kMaximized;
    case PERSISTED_SHOW_STATE_FULLSCREEN:
      return ui::mojom::WindowShowState::kFullscreen;
    case PERSISTED_SHOW_STATE_DETACHED_DEPRECATED:
    case PERSISTED_SHOW_STATE_DOCKED_DEPRECATED:
      return ui::mojom::WindowShowState::kNormal;
  }
  DUMP_WILL_BE_NOTREACHED();
  return ui::mojom::WindowShowState::kNormal;
}

// Iterates through the vector updating the selected_tab_index of each
// SessionWindow based on the actual tabs that were restored.
void UpdateSelectedTabIndex(
    std::vector<std::unique_ptr<SessionWindow>>* windows) {
  for (auto& window : *windows) {
    // See note in SessionWindow as to why we do this.
    int new_index = 0;
    for (auto j = window->tabs.begin(); j != window->tabs.end(); ++j) {
      if ((*j)->tab_visual_index == window->selected_tab_index) {
        new_index = static_cast<int>(j - window->tabs.begin());
        break;
      }
    }
    window->selected_tab_index = new_index;
  }
}

using IdToSessionTab = std::map<SessionID, std::unique_ptr<SessionTab>>;
using IdToSessionWindow = std::map<SessionID, std::unique_ptr<SessionWindow>>;
using GroupIdToSessionTabGroup =
    std::map<tab_groups::TabGroupId, std::unique_ptr<SessionTabGroup>>;

// Returns the window in windows with the specified id. If a window does
// not exist, one is created.
SessionWindow* GetWindow(SessionID window_id, IdToSessionWindow* windows) {
  auto i = windows->find(window_id);
  if (i == windows->end()) {
    SessionWindow* window = new SessionWindow();
    window->window_id = window_id;
    (*windows)[window_id] = base::WrapUnique(window);
    return window;
  }
  return i->second.get();
}

// Returns the tab with the specified id in tabs. If a tab does not exist,
// it is created.
SessionTab* GetTab(SessionID tab_id, IdToSessionTab* tabs) {
  DCHECK(tabs);
  auto i = tabs->find(tab_id);
  if (i == tabs->end()) {
    SessionTab* tab = new SessionTab();
    tab->tab_id = tab_id;
    (*tabs)[tab_id] = base::WrapUnique(tab);
    return tab;
  }
  return i->second.get();
}

SessionTabGroup* GetTabGroup(tab_groups::TabGroupId group_id,
                             GroupIdToSessionTabGroup* groups) {
  DCHECK(groups);
  // For |group_id|, insert a corresponding group entry or get the existing one.
  auto result = groups->emplace(group_id, nullptr);
  GroupIdToSessionTabGroup::iterator it = result.first;
  if (result.second)
    it->second = std::make_unique<SessionTabGroup>(group_id);
  return it->second.get();
}

// Returns an iterator into navigations pointing to the navigation whose
// index matches |index|. If no navigation index matches |index|, the first
// navigation with an index > |index| is returned.
//
// This assumes the navigations are ordered by index in ascending order.
std::vector<sessions::SerializedNavigationEntry>::iterator
  FindClosestNavigationWithIndex(
    std::vector<sessions::SerializedNavigationEntry>* navigations,
    int index) {
  DCHECK(navigations);
  for (auto i = navigations->begin(); i != navigations->end(); ++i) {
    if (i->index() >= index)
      return i;
  }
  return navigations->end();
}

// Function used in sorting windows. Sorting is done based on window id. As
// window ids increment for each new window, this effectively sorts by creation
// time.
static bool WindowOrderSortFunction(const std::unique_ptr<SessionWindow>& w1,
                                    const std::unique_ptr<SessionWindow>& w2) {
  return w1->window_id.id() < w2->window_id.id();
}

// Compares the two tabs based on visual index.
static bool TabVisualIndexSortFunction(const std::unique_ptr<SessionTab>& t1,
                                       const std::unique_ptr<SessionTab>& t2) {
  const int delta = t1->tab_visual_index - t2->tab_visual_index;
  return delta == 0 ? (t1->tab_id.id() < t2->tab_id.id()) : (delta < 0);
}

// Does the following:
// . Deletes and removes any windows with no tabs. NOTE: constrained windows
//   that have been dragged out are of type browser. As such, this preserves any
//   dragged out constrained windows (aka popups that have been dragged out).
// . Sorts the tabs in windows with valid tabs based on the tabs;
//   visual order, and adds the valid windows to |valid_windows|.
void SortTabsBasedOnVisualOrderAndClear(
    IdToSessionWindow* windows,
    std::vector<std::unique_ptr<SessionWindow>>* valid_windows) {
  for (auto& window_pair : *windows) {
    std::unique_ptr<SessionWindow> window = std::move(window_pair.second);
    if (window->tabs.empty() || window->is_constrained) {
      continue;
    } else {
      // Valid window; sort the tabs and add it to the list of valid windows.
      std::sort(window->tabs.begin(), window->tabs.end(),
                &TabVisualIndexSortFunction);
      // Add the window such that older windows appear first.
      if (valid_windows->empty()) {
        valid_windows->push_back(std::move(window));
      } else {
        valid_windows->insert(
            std::upper_bound(valid_windows->begin(), valid_windows->end(),
                             window, &WindowOrderSortFunction),
            std::move(window));
      }
    }
  }

  // There are no more pointers left in |window|, just empty husks from the
  // move, so clear it out.
  windows->clear();
}

// Adds tabs to their parent window based on the tab's window_id. This
// ignores tabs with no navigations.
void AddTabsToWindows(IdToSessionTab* tabs,
                      GroupIdToSessionTabGroup* tab_groups,
                      IdToSessionWindow* windows) {
  DVLOG(1) << "AddTabsToWindows";
  DVLOG(1) << "Tabs " << tabs->size() << ", groups " << tab_groups->size()
           << ", windows " << windows->size();

  for (auto& tab_pair : *tabs) {
    std::unique_ptr<SessionTab> tab = std::move(tab_pair.second);
    if (!tab->window_id.id() || tab->navigations.empty())
      continue;

    SessionTab* tab_ptr = tab.get();
    SessionWindow* window = GetWindow(tab_ptr->window_id, windows);
    window->tabs.push_back(std::move(tab));

    // See note in SessionTab as to why we do this.
    auto j = FindClosestNavigationWithIndex(&tab_ptr->navigations,
                                            tab_ptr->current_navigation_index);
    if (j == tab_ptr->navigations.end()) {
      tab_ptr->current_navigation_index =
          static_cast<int>(tab_ptr->navigations.size() - 1);
    } else {
      tab_ptr->current_navigation_index =
          static_cast<int>(j - tab_ptr->navigations.begin());
    }
  }

  // There are no more pointers left in |tabs|, just empty husks from the
  // move, so clear it out.
  tabs->clear();

  // For each window, collect all the tab groups present. We rely on the fact
  // that tab groups can't be split between windows.
  for (auto& window_pair : *windows) {
    SessionWindow* window = window_pair.second.get();

    base::flat_set<tab_groups::TabGroupId> groups_in_current_window;
    for (const auto& tab : window->tabs) {
      if (tab->group.has_value())
        groups_in_current_window.insert(tab->group.value());
    }

    // Move corresponding SessionTabGroup entries into SessionWindow.
    for (const tab_groups::TabGroupId& group_id : groups_in_current_window) {
      auto it = tab_groups->find(group_id);
      if (it == tab_groups->end()) {
        window->tab_groups.push_back(
            std::make_unique<SessionTabGroup>(group_id));
        continue;
      }
      window->tab_groups.push_back(std::move(it->second));
      tab_groups->erase(it);
    }
  }

  // We may have extraneous tab group entries. Since we don't have explicit
  // commands for opening and closing tab groups, there may be dangling
  // SessionTabGroup entries after all tabs in a group are closed.
  tab_groups->clear();
}

void ProcessTabNavigationPathPrunedCommand(
    TabNavigationPathPrunedPayload& payload,
    SessionTab* tab) {
  // Update the selected navigation index.
  if (tab->current_navigation_index >= payload.index &&
      tab->current_navigation_index < payload.index + payload.count) {
    tab->current_navigation_index = payload.index - 1;
  } else if (tab->current_navigation_index >= payload.index + payload.count) {
    tab->current_navigation_index =
        tab->current_navigation_index - payload.count;
  }  // Else no change if selected index is before payload.index

  tab->navigations.erase(
      FindClosestNavigationWithIndex(&(tab->navigations), payload.index),
      FindClosestNavigationWithIndex(&(tab->navigations),
                                     payload.index + payload.count));

  // And update the index of existing navigations.
  for (auto& entry : tab->navigations) {
    if (entry.index() < payload.index)
      continue;
    entry.set_index(entry.index() - payload.count);
  }
}

// Creates tabs and windows from the commands specified in |data|. The created
// tabs and windows are added to |tabs| and |windows| respectively, with the
// id of the active window set in |active_window_id|. It is up to the caller
// to delete the tabs and windows added to |tabs| and |windows|.
//
// This does NOT add any created SessionTabs to SessionWindow.tabs, that is
// done by AddTabsToWindows.
void CreateTabsAndWindows(
    const std::vector<std::unique_ptr<SessionCommand>>& data,
    IdToSessionTab* tabs,
    GroupIdToSessionTabGroup* tab_groups,
    IdToSessionWindow* windows,
    SessionID* active_window_id) {
  // If the file is corrupt (command with wrong size, or unknown command), we
  // still return true and attempt to restore what we we can.
  DVLOG(1) << "CreateTabsAndWindows";

  for (const auto& command_ptr : data) {
    const SessionCommand::id_type kCommandSetWindowBounds2 = 10;
    const SessionCommand* command = command_ptr.get();

    DVLOG(1) << "Read command " << (int)command->id();
    switch (command->id()) {
      case kCommandSetTabWindow: {
        SessionID::id_type payload[2];
        if (!command->GetPayload(payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionID window_id = SessionID::FromSerializedValue(payload[0]);
        SessionID tab_id = SessionID::FromSerializedValue(payload[1]);
        GetTab(tab_id, tabs)->window_id = window_id;
        break;
      }

      // This is here for forward migration only.  New data is saved with
      // |kCommandSetWindowBounds3|.
      case kCommandSetWindowBounds2: {
        WindowBoundsPayload2 payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionID window_id = SessionID::FromSerializedValue(payload.window_id);
        GetWindow(window_id, windows)
            ->bounds.SetRect(payload.x, payload.y, payload.w, payload.h);
        GetWindow(window_id, windows)->show_state =
            payload.is_maximized ? ui::mojom::WindowShowState::kMaximized
                                 : ui::mojom::WindowShowState::kNormal;
        break;
      }

      case kCommandSetWindowBounds3: {
        WindowBoundsPayload3 payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionID window_id = SessionID::FromSerializedValue(payload.window_id);
        GetWindow(window_id, windows)
            ->bounds.SetRect(payload.x, payload.y, payload.w, payload.h);
        GetWindow(window_id, windows)->show_state =
            PersistedShowStateToShowState(payload.show_state);
        break;
      }

      case kCommandSetTabIndexInWindow: {
        TabIndexInWindowPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionID tab_id = SessionID::FromSerializedValue(payload.id);
        GetTab(tab_id, tabs)->tab_visual_index = payload.index;
        break;
      }

      case kCommandTabClosed:
      case kCommandWindowClosed: {
        ClosedPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        if (command->id() == kCommandTabClosed)
          tabs->erase(SessionID::FromSerializedValue(payload.id));
        else
          windows->erase(SessionID::FromSerializedValue(payload.id));

        break;
      }

      case kCommandTabNavigationPathPrunedFromBack: {
        TabNavigationPathPrunedFromBackPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionTab* tab =
            GetTab(SessionID::FromSerializedValue(payload.id), tabs);

        tab->navigations.erase(
            FindClosestNavigationWithIndex(&(tab->navigations), payload.index),
            tab->navigations.end());
        break;
      }

      case kCommandTabNavigationPathPrunedFromFront: {
        TabNavigationPathPrunedFromFrontPayload prune_front_payload;
        if (!command->GetPayload(&prune_front_payload,
                                 sizeof(prune_front_payload)) ||
            prune_front_payload.index <= 0) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionTab* tab = GetTab(
            SessionID::FromSerializedValue(prune_front_payload.id), tabs);

        TabNavigationPathPrunedPayload payload;
        payload.index = 0;
        payload.count = prune_front_payload.index;
        ProcessTabNavigationPathPrunedCommand(payload, tab);
        break;
      }

      case kCommandTabNavigationPathPruned: {
        TabNavigationPathPrunedPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload)) ||
            payload.index < 0 || payload.count <= 0) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionTab* tab =
            GetTab(SessionID::FromSerializedValue(payload.id), tabs);

        ProcessTabNavigationPathPrunedCommand(payload, tab);
        break;
      }

      case kCommandUpdateTabNavigation: {
        sessions::SerializedNavigationEntry navigation;
        SessionID tab_id = SessionID::InvalidValue();
        if (!RestoreUpdateTabNavigationCommand(*command,
                                               &navigation,
                                               &tab_id)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionTab* tab = GetTab(tab_id, tabs);
        auto i = FindClosestNavigationWithIndex(&(tab->navigations),
                                                navigation.index());
        if (i != tab->navigations.end() && i->index() == navigation.index())
          *i = navigation;
        else
          tab->navigations.insert(i, navigation);
        break;
      }

      case kCommandSetSelectedNavigationIndex: {
        SelectedNavigationIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        GetTab(SessionID::FromSerializedValue(payload.id), tabs)
            ->current_navigation_index = payload.index;
        break;
      }

      case kCommandSetSelectedTabInIndex: {
        SelectedTabInIndexPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        GetWindow(SessionID::FromSerializedValue(payload.id), windows)
            ->selected_tab_index = payload.index;
        break;
      }

      case kCommandSetWindowType: {
        WindowTypePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionID window_id = SessionID::FromSerializedValue(payload.id);
        GetWindow(window_id, windows)->is_constrained = false;
        GetWindow(window_id, windows)->type =
            static_cast<SessionWindow::WindowType>(payload.index);
        break;
      }

      case kCommandSetTabGroup: {
        TabGroupPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionTab* session_tab =
            GetTab(SessionID::FromSerializedValue(payload.tab_id), tabs);
        const base::Token token(payload.maybe_group.id_high,
                                payload.maybe_group.id_low);
        session_tab->group =
            payload.has_group ? std::make_optional(
                                    tab_groups::TabGroupId::FromRawToken(token))
                              : std::nullopt;
        break;
      }

      case kCommandSetTabGroupMetadata2: {
        base::Pickle pickle = command->PayloadAsPickle();
        base::PickleIterator iter(pickle);

        std::optional<base::Token> group_token = ReadTokenFromPickle(&iter);
        if (!group_token.has_value())
          return;

        SessionTabGroup* group = GetTabGroup(
            tab_groups::TabGroupId::FromRawToken(group_token.value()),
            tab_groups);

        std::u16string title;
        if (!iter.ReadString16(&title))
          return;

        uint32_t color_int;
        if (!iter.ReadUInt32(&color_int))
          return;

        // The |is_collapsed| boolean was added in M88 to save the collapsed
        // state, so previous versions may not have this stored.
        bool is_collapsed = false;
        std::ignore = iter.ReadBool(&is_collapsed);
        group->visual_data =
            tab_groups::TabGroupVisualData(title, color_int, is_collapsed);

        // The |is_saved| boolean was added in M113 to save the saved state of a
        // tab group. Previous versions may not have this stored.
        bool is_saved = false;
        std::ignore = iter.ReadBool(&is_saved);

        if (is_saved) {
          // The |saved_guid| boolean was added in M113 to save the guid of a
          // tab group. Previous version may not have this stored.
          std::string saved_guid;
          if (!iter.ReadString(&saved_guid)) {
            return;
          }
          group->saved_guid = saved_guid;
        } else {
          // Explicitly update the |saved_guid| to nullopt if the group
          // isn't saved. This is to ensure the right value is set when there
          // are multiple entries in the append log file.
          group->saved_guid = std::nullopt;
        }

        break;
      }

      case kCommandSetPinnedState: {
        PinnedStatePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        GetTab(SessionID::FromSerializedValue(payload.tab_id), tabs)->pinned =
            payload.pinned_state;
        break;
      }

      case kCommandSetWindowAppName: {
        SessionID window_id = SessionID::InvalidValue();
        std::string app_name;
        if (!RestoreSetWindowAppNameCommand(*command, &window_id, &app_name))
          return;

        GetWindow(window_id, windows)->app_name.swap(app_name);
        break;
      }

      case kCommandSetExtensionAppID: {
        SessionID tab_id = SessionID::InvalidValue();
        std::string extension_app_id;
        if (!RestoreSetTabExtensionAppIDCommand(*command,
                                                &tab_id,
                                                &extension_app_id)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }

        GetTab(tab_id, tabs)->extension_app_id.swap(extension_app_id);
        break;
      }

      case kCommandSetTabUserAgentOverride: {
        SessionID tab_id = SessionID::InvalidValue();
        std::string user_agent_override;
        if (!RestoreSetTabUserAgentOverrideCommand(
                *command,
                &tab_id,
                &user_agent_override)) {
          return;
        }

        SessionTab* tab = GetTab(tab_id, tabs);
        tab->user_agent_override.ua_string_override.swap(user_agent_override);
        tab->user_agent_override.opaque_ua_metadata_override = std::nullopt;
        break;
      }

      case kCommandSetTabUserAgentOverride2: {
        SessionID tab_id = SessionID::InvalidValue();
        std::string user_agent_override;
        std::optional<std::string> opaque_ua_metadata_override;
        if (!RestoreSetTabUserAgentOverrideCommand2(
                *command, &tab_id, &user_agent_override,
                &opaque_ua_metadata_override)) {
          return;
        }
        SessionTab* tab = GetTab(tab_id, tabs);
        tab->user_agent_override.ua_string_override =
            std::move(user_agent_override);
        tab->user_agent_override.opaque_ua_metadata_override =
            std::move(opaque_ua_metadata_override);
        break;
      }

      case kCommandSessionStorageAssociated: {
        base::Pickle command_pickle = command->PayloadAsPickle();
        base::PickleIterator iter(command_pickle);
        SessionID::id_type command_tab_id;
        std::string session_storage_persistent_id;
        if (!iter.ReadInt(&command_tab_id) ||
            !iter.ReadString(&session_storage_persistent_id))
          return;
        // Associate the session storage back.
        GetTab(SessionID::FromSerializedValue(command_tab_id), tabs)
            ->session_storage_persistent_id = session_storage_persistent_id;
        break;
      }

      case kCommandSetActiveWindow: {
        ActiveWindowPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        *active_window_id = SessionID::FromSerializedValue(payload);
        break;
      }

      case kCommandLastActiveTime: {
        LastActiveTimePayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        SessionTab* tab =
            GetTab(SessionID::FromSerializedValue(payload.tab_id), tabs);
        base::Time deserialized_time = base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(payload.last_active_time));

        if (base::Time::Now() - deserialized_time >
            kLastActiveWorkaroundThreshold) {
          // TODO(crbug.com/40946710): Remove this once enough time has passed
          // (added in December 2023, can be removed after ~1 year). This is a
          // workaround put in place during the migration from base::TimeTicks
          // internal representation to microseconds since Windows epoch. As the
          // origin point may be vastely different, the values stored in the old
          // format appear as really old when deserialized in the new format. So
          // checking all value older than 15 years should be a good enough
          // filter to catch them. If it is a value stored in the old format, it
          // should be correctly decoded.
          base::TimeTicks time_tick_value =
              base::TimeTicks::FromInternalValue(payload.last_active_time);
          base::TimeDelta delta_since_epoch =
              time_tick_value - base::TimeTicks::UnixEpoch();
          base::Time corrected_time =
              base::Time::UnixEpoch() + delta_since_epoch;
          if (base::Time::Now() < corrected_time) {
            // If the correction is giving a time in the future, set it to now.
            corrected_time = base::Time::Now();
          }
          deserialized_time = corrected_time;
        }
        tab->last_active_time = deserialized_time;
        break;
      }

      case kCommandSetWindowWorkspace2: {
        base::Pickle pickle = command->PayloadAsPickle();
        base::PickleIterator it(pickle);
        SessionID::id_type window_id = -1;
        std::string workspace;
         if (!it.ReadInt(&window_id) || !it.ReadString(&workspace)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        GetWindow(SessionID::FromSerializedValue(window_id), windows)
            ->workspace = workspace;
        break;
      }

      case kCommandSetWindowVisibleOnAllWorkspaces: {
        VisibleOnAllWorkspacesPayload payload;
        if (!command->GetPayload(&payload, sizeof(payload))) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        GetWindow(SessionID::FromSerializedValue(payload.window_id), windows)
            ->visible_on_all_workspaces = payload.visible_on_all_workspaces;
        break;
      }

      case kCommandSetTabGuid: {
        base::Pickle pickle = command->PayloadAsPickle();
        base::PickleIterator it(pickle);
        SessionID::id_type tab_id = -1;
        std::string guid;
        if (!it.ReadInt(&tab_id) || !it.ReadString(&guid) ||
            !base::Uuid::ParseCaseInsensitive(guid).is_valid()) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        GetTab(SessionID::FromSerializedValue(tab_id), tabs)->guid = guid;
        break;
      }

      case kCommandSetTabData: {
        base::Pickle pickle = command->PayloadAsPickle();
        base::PickleIterator it(pickle);
        SessionID::id_type tab_id = -1;
        int size = 0;
        if (!it.ReadInt(&tab_id) || !it.ReadInt(&size)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }
        std::map<std::string, std::string> tab_data;
        for (int i = 0; i < size; i++) {
          std::string key;
          std::string value;
          if (!it.ReadString(&key) || !it.ReadString(&value)) {
            DVLOG(1) << "Failed reading command " << command->id();
            return;
          }
          tab_data.insert({key, value});
        }

        GetTab(SessionID::FromSerializedValue(tab_id), tabs)->data =
            std::move(tab_data);
        break;
      }

      case kCommandAddTabExtraData: {
        SessionID tab_id = SessionID::InvalidValue();
        std::string key;
        std::string extra_data;
        if (!RestoreAddExtraDataCommand(*command, &tab_id, &key, &extra_data)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }

        GetTab(tab_id, tabs)->extra_data[key] = std::move(extra_data);
        break;
      }

      case kCommandAddWindowExtraData: {
        SessionID window_id = SessionID::InvalidValue();
        std::string key;
        std::string extra_data;
        if (!RestoreAddExtraDataCommand(*command, &window_id, &key,
                                        &extra_data)) {
          DVLOG(1) << "Failed reading command " << command->id();
          return;
        }

        GetWindow(window_id, windows)->extra_data[key] = std::move(extra_data);
        break;
      }

      case kCommandSetWindowUserTitle: {
        SessionID window_id = SessionID::InvalidValue();
        std::string title;
        if (!RestoreSetWindowUserTitleCommand(*command, &window_id, &title))
          return;
        GetWindow(window_id, windows)->user_title = title;
        break;
      }

      default:
        DVLOG(1) << "Failed reading an unknown command " << command->id();
        return;
    }
  }
}

template <typename Payload>
std::unique_ptr<SessionCommand> CreateSessionCommandForPayload(
    SessionCommand::id_type id,
    const Payload& payload) {
  auto command = std::make_unique<SessionCommand>(id, sizeof(payload));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

}  // namespace

std::unique_ptr<SessionCommand> CreateSetSelectedTabInWindowCommand(
    SessionID window_id,
    int index) {
  SelectedTabInIndexPayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = index;
  return CreateSessionCommandForPayload(kCommandSetSelectedTabInIndex, payload);
}

std::unique_ptr<SessionCommand> CreateSetTabWindowCommand(SessionID window_id,
                                                          SessionID tab_id) {
  SessionID::id_type payload[] = { window_id.id(), tab_id.id() };
  return CreateSessionCommandForPayload(kCommandSetTabWindow, payload);
}

std::unique_ptr<SessionCommand> CreateSetWindowBoundsCommand(
    SessionID window_id,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state) {
  WindowBoundsPayload3 payload = { 0 };
  payload.window_id = window_id.id();
  payload.x = bounds.x();
  payload.y = bounds.y();
  payload.w = bounds.width();
  payload.h = bounds.height();
  payload.show_state = ShowStateToPersistedShowState(show_state);
  return CreateSessionCommandForPayload(kCommandSetWindowBounds3, payload);
}

std::unique_ptr<SessionCommand> CreateSetTabIndexInWindowCommand(
    SessionID tab_id,
    int new_index) {
  TabIndexInWindowPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = new_index;
  return CreateSessionCommandForPayload(kCommandSetTabIndexInWindow, payload);
}

std::unique_ptr<SessionCommand> CreateTabClosedCommand(const SessionID tab_id) {
  ClosedPayload payload;
  // Because of what appears to be a compiler bug setting payload to {0} doesn't
  // set the padding to 0, resulting in Purify reporting an UMR when we write
  // the structure to disk. To avoid this we explicitly memset the struct.
  memset(&payload, 0, sizeof(payload));
  payload.id = tab_id.id();
  payload.close_time = base::Time::Now().ToInternalValue();
  return CreateSessionCommandForPayload(kCommandTabClosed, payload);
}

std::unique_ptr<SessionCommand> CreateWindowClosedCommand(
    const SessionID window_id) {
  ClosedPayload payload;
  // See comment in CreateTabClosedCommand as to why we do this.
  memset(&payload, 0, sizeof(payload));
  payload.id = window_id.id();
  payload.close_time = base::Time::Now().ToInternalValue();
  return CreateSessionCommandForPayload(kCommandWindowClosed, payload);
}

std::unique_ptr<SessionCommand> CreateSetSelectedNavigationIndexCommand(
    SessionID tab_id,
    int index) {
  SelectedNavigationIndexPayload payload = { 0 };
  payload.id = tab_id.id();
  payload.index = index;
  return CreateSessionCommandForPayload(kCommandSetSelectedNavigationIndex,
                                        payload);
}

std::unique_ptr<SessionCommand> CreateSetWindowTypeCommand(
    SessionID window_id,
    SessionWindow::WindowType type) {
  WindowTypePayload payload = { 0 };
  payload.id = window_id.id();
  payload.index = static_cast<int32_t>(type);
  return CreateSessionCommandForPayload(kCommandSetWindowType, payload);
}

std::unique_ptr<SessionCommand> CreateTabGroupCommand(
    SessionID tab_id,
    std::optional<tab_groups::TabGroupId> group) {
  TabGroupPayload payload = {0};
  payload.tab_id = tab_id.id();
  if (group.has_value()) {
    DCHECK(!group.value().token().is_zero());
    payload.maybe_group.id_high = group.value().token().high();
    payload.maybe_group.id_low = group.value().token().low();
    payload.has_group = true;
  }
  return CreateSessionCommandForPayload(kCommandSetTabGroup, payload);
}

std::unique_ptr<SessionCommand> CreateTabGroupMetadataUpdateCommand(
    const tab_groups::TabGroupId group,
    const tab_groups::TabGroupVisualData* visual_data,
    const std::optional<std::string> saved_guid) {
  base::Pickle pickle;
  WriteTokenToPickle(&pickle, group.token());
  pickle.WriteString16(visual_data->title());
  pickle.WriteUInt32(static_cast<int>(visual_data->color()));

  // This boolean was added in M88 to save the collapsed state.
  pickle.WriteBool(visual_data->is_collapsed());

  // This booleans was added in M113 to save the saved state of a tab group.
  pickle.WriteBool(saved_guid.has_value());
  if (saved_guid.has_value()) {
    // This string was  added in M113 to save the guid of a tab group.
    pickle.WriteString(saved_guid.value());
  }

  return std::make_unique<SessionCommand>(kCommandSetTabGroupMetadata2, pickle);
}

std::unique_ptr<SessionCommand> CreatePinnedStateCommand(SessionID tab_id,
                                                         bool is_pinned) {
  PinnedStatePayload payload = { 0 };
  payload.tab_id = tab_id.id();
  payload.pinned_state = is_pinned;
  return CreateSessionCommandForPayload(kCommandSetPinnedState, payload);
}

std::unique_ptr<SessionCommand> CreateSessionStorageAssociatedCommand(
    SessionID tab_id,
    const std::string& session_storage_persistent_id) {
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());
  pickle.WriteString(session_storage_persistent_id);
  return std::make_unique<SessionCommand>(kCommandSessionStorageAssociated,
                                          pickle);
}

std::unique_ptr<SessionCommand> CreateSetActiveWindowCommand(
    SessionID window_id) {
  ActiveWindowPayload payload = 0;
  payload = window_id.id();
  return CreateSessionCommandForPayload(kCommandSetActiveWindow, payload);
}

std::unique_ptr<SessionCommand> CreateLastActiveTimeCommand(
    SessionID tab_id,
    base::Time last_active_time) {
  LastActiveTimePayload payload = {0};
  payload.tab_id = tab_id.id();
  payload.last_active_time =
      last_active_time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  return CreateSessionCommandForPayload(kCommandLastActiveTime, payload);
}

std::unique_ptr<SessionCommand> CreateSetWindowWorkspaceCommand(
    SessionID window_id,
    const std::string& workspace) {
  base::Pickle pickle;
  pickle.WriteInt(window_id.id());
  pickle.WriteString(workspace);
  return std::make_unique<SessionCommand>(kCommandSetWindowWorkspace2, pickle);
}

std::unique_ptr<SessionCommand> CreateSetWindowVisibleOnAllWorkspacesCommand(
    SessionID window_id,
    bool visible_on_all_workspaces) {
  VisibleOnAllWorkspacesPayload payload = {0};
  payload.window_id = window_id.id();
  payload.visible_on_all_workspaces = visible_on_all_workspaces;
  return CreateSessionCommandForPayload(kCommandSetWindowVisibleOnAllWorkspaces,
                                        payload);
}

std::unique_ptr<SessionCommand>
CreateTabNavigationPathPrunedCommand(SessionID tab_id, int index, int count) {
  TabNavigationPathPrunedPayload payload = {0};
  payload.id = tab_id.id();
  payload.index = index;
  payload.count = count;
  return CreateSessionCommandForPayload(kCommandTabNavigationPathPruned,
                                        payload);
}

std::unique_ptr<SessionCommand> CreateUpdateTabNavigationCommand(
    SessionID tab_id,
    const sessions::SerializedNavigationEntry& navigation) {
  return CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation, tab_id,
                                          navigation);
}

std::unique_ptr<SessionCommand> CreateSetTabExtensionAppIDCommand(
    SessionID tab_id,
    const std::string& extension_id) {
  return CreateSetTabExtensionAppIDCommand(kCommandSetExtensionAppID, tab_id,
                                           extension_id);
}

std::unique_ptr<SessionCommand> CreateSetTabUserAgentOverrideCommand(
    SessionID tab_id,
    const SerializedUserAgentOverride& user_agent_override) {
  return CreateSetTabUserAgentOverrideCommand(kCommandSetTabUserAgentOverride2,
                                              tab_id, user_agent_override);
}

std::unique_ptr<SessionCommand> CreateSetWindowAppNameCommand(
    SessionID window_id,
    const std::string& app_name) {
  return CreateSetWindowAppNameCommand(kCommandSetWindowAppName, window_id,
                                       app_name);
}

std::unique_ptr<SessionCommand> CreateSetWindowUserTitleCommand(
    SessionID window_id,
    const std::string& user_title) {
  return CreateSetWindowUserTitleCommand(kCommandSetWindowUserTitle, window_id,
                                         user_title);
}

std::unique_ptr<SessionCommand> CreateSetTabGuidCommand(
    SessionID tab_id,
    const std::string& guid) {
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());
  pickle.WriteString(guid);
  return std::make_unique<SessionCommand>(kCommandSetTabGuid, pickle);
}

std::unique_ptr<SessionCommand> CreateSetTabDataCommand(
    SessionID tab_id,
    const std::map<std::string, std::string>& data) {
  base::Pickle pickle;
  pickle.WriteInt(tab_id.id());
  pickle.WriteInt(data.size());
  for (const auto& kv : data) {
    pickle.WriteString(kv.first);
    pickle.WriteString(kv.second);
  }
  return std::make_unique<SessionCommand>(kCommandSetTabData, pickle);
}

std::unique_ptr<SessionCommand> CreateAddTabExtraDataCommand(
    SessionID tab_id,
    const std::string& key,
    const std::string& data) {
  return CreateAddExtraDataCommand(kCommandAddTabExtraData, tab_id, key, data);
}

std::unique_ptr<SessionCommand> CreateAddWindowExtraDataCommand(
    SessionID window_id,
    const std::string& key,
    const std::string& data) {
  return CreateAddExtraDataCommand(kCommandAddWindowExtraData, window_id, key,
                                   data);
}

bool ReplacePendingCommand(CommandStorageManager* command_storage_manager,
                           std::unique_ptr<SessionCommand>* command) {
  // We optimize page navigations, which can happen quite frequently and
  // is expensive. And activation is like Highlander, there can only be one!
  if ((*command)->id() != kCommandUpdateTabNavigation &&
      (*command)->id() != kCommandSetActiveWindow) {
    return false;
  }
  for (auto i = command_storage_manager->pending_commands().rbegin();
       i != command_storage_manager->pending_commands().rend(); ++i) {
    SessionCommand* existing_command = i->get();
    if ((*command)->id() == kCommandUpdateTabNavigation &&
        existing_command->id() == kCommandUpdateTabNavigation) {
      base::Pickle command_pickle = (*command)->PayloadAsPickle();
      base::PickleIterator iterator(command_pickle);
      SessionID::id_type command_tab_id;
      int command_nav_index;
      if (!iterator.ReadInt(&command_tab_id) ||
          !iterator.ReadInt(&command_nav_index)) {
        return false;
      }
      SessionID::id_type existing_tab_id;
      int existing_nav_index;
      {
        // Creating a pickle like this means the Pickle references the data from
        // the command. Make sure we delete the pickle before the command, else
        // the pickle references deleted memory.
        base::Pickle existing_pickle = existing_command->PayloadAsPickle();
        iterator = base::PickleIterator(existing_pickle);
        if (!iterator.ReadInt(&existing_tab_id) ||
            !iterator.ReadInt(&existing_nav_index)) {
          return false;
        }
      }
      if (existing_tab_id == command_tab_id &&
          existing_nav_index == command_nav_index) {
        // existing_command is an update for the same tab/index pair. Replace
        // it with the new one. We need to add to the end of the list just in
        // case there is a prune command after the update command.
        command_storage_manager->EraseCommand((i.base() - 1)->get());
        command_storage_manager->AppendRebuildCommand(std::move(*command));
        return true;
      }
      return false;
    }
    if ((*command)->id() == kCommandSetActiveWindow &&
        existing_command->id() == kCommandSetActiveWindow) {
      command_storage_manager->SwapCommand(existing_command,
                                           (std::move(*command)));
      return true;
    }
  }
  return false;
}

bool IsClosingCommand(SessionCommand* command) {
  return command->id() == kCommandTabClosed ||
         command->id() == kCommandWindowClosed;
}

void RestoreSessionFromCommands(
    const std::vector<std::unique_ptr<SessionCommand>>& commands,
    std::vector<std::unique_ptr<SessionWindow>>* valid_windows,
    SessionID* active_window_id) {
  IdToSessionTab tabs;
  GroupIdToSessionTabGroup tab_groups;
  IdToSessionWindow windows;

  DVLOG(1) << "RestoreSessionFromCommands " << commands.size();
  CreateTabsAndWindows(commands, &tabs, &tab_groups, &windows,
                       active_window_id);
  AddTabsToWindows(&tabs, &tab_groups, &windows);
  SortTabsBasedOnVisualOrderAndClear(&windows, valid_windows);
  UpdateSelectedTabIndex(valid_windows);
  // After processing, all windows should have at least one tab, and each
  // tab should have at least one navigation.
#if DCHECK_IS_ON()
  for (const auto& window : *valid_windows) {
    DCHECK(!window->tabs.empty());
    for (const auto& tab : window->tabs)
      DCHECK(!tab->navigations.empty());
  }
#endif
  // AddTabsToWindows should have processed all the tabs and groups.
  DCHECK_EQ(0u, tabs.size());
  DCHECK_EQ(0u, tab_groups.size());
  // SortTabsBasedOnVisualOrderAndClear should have processed all the windows.
  DCHECK_EQ(0u, windows.size());
}

}  // namespace sessions
