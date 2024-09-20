// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/base_session_service_commands.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom.h"

#undef LoadBitmap

namespace sessions {

namespace {

// Only written if the tab is pinned.
typedef bool PinnedStatePayload;

typedef int32_t RestoredEntryPayload;

// Payload used for the start of a tab close. This is the old struct that is
// used for backwards compat when it comes to reading the session files.
struct SelectedNavigationInTabPayload {
  SessionID::id_type id;
  int32_t index;
};

// Payload used for the start of a window close. This is the old struct that is
// used for backwards compat when it comes to reading the session files. This
// struct must be POD, because we memset the contents.
struct WindowPayloadObsolete {
  SessionID::id_type window_id;
  int32_t selected_tab_index;
  int32_t num_tabs;
};

// Payload used for the start of a window close. This struct must be POD,
// because we memset the contents. This is an older version of the struct that
// is used for backwards compat when it comes to reading the session files.
struct WindowPayloadObsolete2 : WindowPayloadObsolete {
  int64_t timestamp;
};

// Payload used for the start of a tab close.
struct SelectedNavigationInTabPayload2 : SelectedNavigationInTabPayload {
  int64_t timestamp;
};

// Used to indicate what has loaded.
enum LoadState {
  // Indicates we haven't loaded anything.
  NOT_LOADED = 1 << 0,

  // Indicates we've asked for the last sessions and tabs but haven't gotten the
  // result back yet.
  LOADING = 1 << 2,

  // Indicates we finished loading the last tabs (but not necessarily the last
  // session).
  LOADED_LAST_TABS = 1 << 3,

  // Indicates we finished loading the last session (but not necessarily the
  // last tabs).
  LOADED_LAST_SESSION = 1 << 4
};

// Identifier for commands written to file. The ordering in the file is as
// follows:
// . When the user closes a tab a command of type
//   kCommandSelectedNavigationInTab is written identifying the tab and
//   the selected index, then a kCommandPinnedState command if the tab was
//   pinned and kCommandSetExtensionAppID if the tab has an app id and
//   the user agent override if it was using one.  This is
//   followed by any number of kCommandUpdateTabNavigation commands (1 per
//   navigation entry).
// . When the user closes a window a kCommandSelectedNavigationInTab command
//   is written out and followed by n tab closed sequences (as previoulsy
//   described).
// . When the user restores an entry a command of type kCommandRestoredEntry
//   is written.
const SessionCommand::id_type kCommandUpdateTabNavigation = 1;
const SessionCommand::id_type kCommandRestoredEntry = 2;
const SessionCommand::id_type kCommandWindowDeprecated = 3;
const SessionCommand::id_type kCommandSelectedNavigationInTab = 4;
const SessionCommand::id_type kCommandPinnedState = 5;
const SessionCommand::id_type kCommandSetExtensionAppID = 6;
const SessionCommand::id_type kCommandSetWindowAppName = 7;
// Deprecated for kCommandSetTabUserAgentOverride2
const SessionCommand::id_type kCommandSetTabUserAgentOverride = 8;
const SessionCommand::id_type kCommandWindow = 9;
const SessionCommand::id_type kCommandSetTabGroupData = 10;
const SessionCommand::id_type kCommandSetTabUserAgentOverride2 = 11;
const SessionCommand::id_type kCommandSetWindowUserTitle = 12;
const SessionCommand::id_type kCommandCreateGroup = 13;
const SessionCommand::id_type kCommandAddTabExtraData = 14;

// Number of entries (not commands) before we clobber the file and write
// everything.
const int kEntriesPerReset = 40;

const size_t kMaxEntries = TabRestoreServiceHelper::kMaxEntries;

void RemoveEntryByID(
    SessionID id,
    std::vector<std::unique_ptr<tab_restore::Entry>>* entries) {
  // If the id is invalid, return.
  if (id == SessionID::InvalidValue()) {
    return;
  }  // Look for the entry in the top-level collection.
  for (auto entry_it = entries->begin(); entry_it != entries->end();
       ++entry_it) {
    tab_restore::Entry& entry = **entry_it;
    // Erase it if it's our target.
    if (entry.id == id || entry.original_id == id) {
      entries->erase(entry_it);
      return;
    }
    // If this entry is a window, look through its tabs.
    if (entry.type == tab_restore::Type::WINDOW) {
      auto& window = static_cast<tab_restore::Window&>(entry);
      for (auto it = window.tabs.begin(); it != window.tabs.end(); ++it) {
        const tab_restore::Tab& tab = **it;
        // Erase it if it's our target.
        if (tab.id == id) {
          window.tabs.erase(it);
          return;
        }
      }
    }
    // If this entry is a group, look through its tabs.
    if (entry.type == tab_restore::Type::GROUP) {
      auto& group = static_cast<tab_restore::Group&>(entry);
      for (auto it = group.tabs.begin(); it != group.tabs.end(); ++it) {
        const tab_restore::Tab& tab = **it;
        // Erase it if it's our target.
        if (tab.id == id) {
          group.tabs.erase(it);
          return;
        }
      }
    }
  }
}

// An enum that corresponds to ui::WindowShowStates. This needs to be kept in
// sync with that enum. Moreover, the integer values corresponding to each show
// state need to be stable in this enum (which is not necessarily true about the
// ui::WindowShowStates enum).
enum SerializedWindowShowState : int {
  kSerializedShowStateInvalid = -1,
  kSerializedShowStateDefault = 0,
  kSerializedShowStateNormal = 1,
  kSerializedShowStateMinimized = 2,
  kSerializedShowStateMaximized = 3,
  kSerializedShowStateInactive = 4,
  kSerializedShowStateFullscreen = 5,
};

// Converts a window show state to an integer. This function needs to be kept
// up to date with the SerializedWindowShowState enum.
int SerializeWindowShowState(ui::mojom::WindowShowState show_state) {
  switch (show_state) {
    case ui::mojom::WindowShowState::kDefault:
      return kSerializedShowStateDefault;
    case ui::mojom::WindowShowState::kNormal:
      return kSerializedShowStateNormal;
    case ui::mojom::WindowShowState::kMinimized:
      return kSerializedShowStateMinimized;
    case ui::mojom::WindowShowState::kMaximized:
      return kSerializedShowStateMaximized;
    case ui::mojom::WindowShowState::kInactive:
      return kSerializedShowStateInactive;
    case ui::mojom::WindowShowState::kFullscreen:
      return kSerializedShowStateFullscreen;
    case ui::mojom::WindowShowState::kEnd:
      // This should never happen.
      NOTREACHED_IN_MIGRATION();
  }
  return kSerializedShowStateInvalid;
}

// Converts an integer to a window show state. Returns true on success, false
// otherwise. This function needs to be kept up to date with the
// SerializedWindowShowState enum.
bool DeserializeWindowShowState(int show_state_int,
                                ui::mojom::WindowShowState* show_state) {
  switch (static_cast<SerializedWindowShowState>(show_state_int)) {
    case kSerializedShowStateDefault:
      *show_state = ui::mojom::WindowShowState::kDefault;
      return true;
    case kSerializedShowStateNormal:
      *show_state = ui::mojom::WindowShowState::kNormal;
      return true;
    case kSerializedShowStateMinimized:
      *show_state = ui::mojom::WindowShowState::kMinimized;
      return true;
    case kSerializedShowStateMaximized:
      *show_state = ui::mojom::WindowShowState::kMaximized;
      return true;
    case kSerializedShowStateInactive:
      *show_state = ui::mojom::WindowShowState::kInactive;
      return true;
    case kSerializedShowStateFullscreen:
      *show_state = ui::mojom::WindowShowState::kFullscreen;
      return true;
    case kSerializedShowStateInvalid:
    default:
      // Ignore unknown values. This could happen if the data is corrupt.
      break;
  }
  return false;
}

// Converts an int to a window type. Returns true on success, false otherwise.
bool DeserializeWindowType(int type_int,
                           sessions::SessionWindow::WindowType* type) {
  switch (static_cast<sessions::SessionWindow::WindowType>(type_int)) {
    case sessions::SessionWindow::TYPE_NORMAL:
    case sessions::SessionWindow::TYPE_POPUP:
    case sessions::SessionWindow::TYPE_APP:
    case sessions::SessionWindow::TYPE_DEVTOOLS:
    case sessions::SessionWindow::TYPE_APP_POPUP:
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case sessions::SessionWindow::TYPE_CUSTOM_TAB:
#endif
      *type = static_cast<sessions::SessionWindow::WindowType>(type_int);
      return true;
  }
  return false;
}

// Superset of WindowPayloadObsolete/WindowPayloadObsolete2 and the other fields
// that can appear in the Pickle version of a Window command. This is used as a
// convenient destination for parsing the various fields in a WindowCommand.
struct WindowCommandFields {
  // Fields in WindowPayloadObsolete/WindowPayloadObsolete2/Pickle:
  int window_id = 0;
  int selected_tab_index = 0;
  int num_tabs = 0;

  // Fields in WindowPayloadObsolete2/Pickle:
  int64_t timestamp = 0;

  // Fields in Pickle:
  // Completely zeroed position/dimensions indicates that defaults should be
  // used.
  int window_x = 0;
  int window_y = 0;
  int window_width = 0;
  int window_height = 0;
  int window_show_state = 0;
  std::string workspace;

  int type = 0;
};

std::unique_ptr<sessions::tab_restore::Window> CreateWindowEntryFromCommand(
    const SessionCommand* command,
    SessionID* window_id,
    int32_t* num_tabs) {
  WindowCommandFields fields;
  ui::mojom::WindowShowState show_state = ui::mojom::WindowShowState::kDefault;
  auto type = sessions::SessionWindow::TYPE_NORMAL;

  if (command->id() == kCommandWindow) {
    base::Pickle pickle = command->PayloadAsPickle();
    base::PickleIterator it(pickle);
    WindowCommandFields parsed_fields;

    // The first version of the pickle contains all of the following fields, so
    // they should all successfully parse if the command is in fact a pickle.
    if (!it.ReadInt(&parsed_fields.window_id) ||
        !it.ReadInt(&parsed_fields.selected_tab_index) ||
        !it.ReadInt(&parsed_fields.num_tabs) ||
        !it.ReadInt64(&parsed_fields.timestamp) ||
        !it.ReadInt(&parsed_fields.window_x) ||
        !it.ReadInt(&parsed_fields.window_y) ||
        !it.ReadInt(&parsed_fields.window_width) ||
        !it.ReadInt(&parsed_fields.window_height) ||
        !it.ReadInt(&parsed_fields.window_show_state) ||
        !it.ReadString(&parsed_fields.workspace)) {
      return nullptr;
    }

    // New field in M104, use default if it fails to read.
    // TODO(crbug.com/40845240): After some time (say M114), this code can be
    // added into parsing above which fails when ReadInt() fails.
    if (!it.ReadInt(&parsed_fields.type)) {
      parsed_fields.type =
          static_cast<int>(sessions::SessionWindow::TYPE_NORMAL);
    }

    // Validate the parameters. If the entire pickles parses but any of the
    // validation fails assume corruption.
    if (parsed_fields.window_width < 0 || parsed_fields.window_height < 0) {
      return nullptr;
    }
    // Deserialize the show state, validating it at the same time.
    if (!DeserializeWindowShowState(parsed_fields.window_show_state,
                                    &show_state)) {
      return nullptr;
    }

    // Validate window type.
    if (!DeserializeWindowType(parsed_fields.type, &type)) {
      return nullptr;
    }
    // New fields added to the pickle in later versions would be parsed and
    // validated here.

    // Copy the parsed data.
    fields = parsed_fields;
  } else if (command->id() == kCommandWindowDeprecated) {
    // Old window commands can be in either of 2 formats. Try the newest first.
    // These have distinct sizes so are easily distinguished.
    bool parsed = false;

    // Try to parse the command as a WindowPayloadObsolete2.
    WindowPayloadObsolete2 payload2;
    if (command->GetPayload(&payload2, sizeof(payload2))) {
      fields.window_id = payload2.window_id;
      fields.selected_tab_index = payload2.selected_tab_index;
      fields.num_tabs = payload2.num_tabs;
      fields.timestamp = payload2.timestamp;
      parsed = true;
    }

    // Finally, try the oldest WindowPayloadObsolete type.
    if (!parsed) {
      WindowPayloadObsolete payload;
      if (command->GetPayload(&payload, sizeof(payload))) {
        fields.window_id = payload.window_id;
        fields.selected_tab_index = payload.selected_tab_index;
        fields.num_tabs = payload.num_tabs;
        parsed = true;
      }
    }

    // Fail if the old command wasn't able to be parsed in either of the
    // deprecated formats.
    if (!parsed) {
      return nullptr;
    }
  } else {
    // This should never be called with anything other than a known window
    // command ID.
    NOTREACHED_IN_MIGRATION();
  }

  // Create the Window entry.
  std::unique_ptr<sessions::tab_restore::Window> window =
      std::make_unique<sessions::tab_restore::Window>();
  window->type = type;
  window->selected_tab_index = fields.selected_tab_index;
  window->timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(fields.timestamp));
  *window_id = SessionID::FromSerializedValue(fields.window_id);
  *num_tabs = fields.num_tabs;

  // Set the bounds, show state and workspace if valid ones have been provided.
  if (!(fields.window_x == 0 && fields.window_y == 0 &&
        fields.window_width == 0 && fields.window_height == 0)) {
    window->bounds.SetRect(fields.window_x, fields.window_y,
                           fields.window_width, fields.window_height);
  }

  // |show_state| was converted from window->show_state earlier during
  // validation.
  window->show_state = show_state;
  window->workspace = std::move(fields.workspace);
  return window;
}

// Fields that can appear in the Pickle version of a Group command. This is
// used as a convenient destination for parsing the various fields in a
// CreateGroupCommand.
struct GroupCommandFields {
  std::optional<base::Token> tab_group_token;
  int session_id = 0;
  int num_tabs = 0;
  int browser_id = 0;
  std::u16string title;
  uint32_t color = 0;
  bool is_saved;
  std::string saved_id;
};

std::unique_ptr<sessions::tab_restore::Group> CreateGroupEntryFromCommand(
    const SessionCommand* command,
    SessionID* session_id,
    int32_t* num_tabs) {
  base::Pickle pickle = command->PayloadAsPickle();
  base::PickleIterator it(pickle);
  GroupCommandFields parsed_fields;

  // The first version of the pickle contains all of the following fields, so
  // they should all successfully parse if the command is in fact a pickle.
  parsed_fields.tab_group_token = ReadTokenFromPickle(&it);
  if (!parsed_fields.tab_group_token.has_value()) {
    return nullptr;
  }
  if (!it.ReadInt(&parsed_fields.session_id) ||
      !it.ReadInt(&parsed_fields.num_tabs) ||
      !it.ReadInt(&parsed_fields.browser_id) ||
      !it.ReadString16(&parsed_fields.title) ||
      !it.ReadUInt32(&parsed_fields.color)) {
    return nullptr;
  }

  if (it.ReadBool(&parsed_fields.is_saved) && parsed_fields.is_saved) {
    if (!it.ReadString(&parsed_fields.saved_id) ||
        parsed_fields.saved_id.empty()) {
      // A saved group must have a saved id.
      return nullptr;
    }
  }

  // Copy the parsed data.
  GroupCommandFields fields = parsed_fields;

  // Create the Group entry.
  std::unique_ptr<sessions::tab_restore::Group> group =
      std::make_unique<sessions::tab_restore::Group>();
  group->group_id =
      tab_groups::TabGroupId::FromRawToken(fields.tab_group_token.value());
  if (fields.is_saved) {
    group->saved_group_id = base::Uuid::ParseLowercase(fields.saved_id);
  }

  group->browser_id = fields.browser_id;
  group->visual_data =
      tab_groups::TabGroupVisualData(fields.title, fields.color);
  *session_id = SessionID::FromSerializedValue(fields.session_id);
  *num_tabs = fields.num_tabs;

  return group;
}

}  // namespace

// TabRestoreServiceImpl::PersistenceDelegate
// ---------------------------------------

// This restore service persistence delegate will create and own a
// CommandStorageManager and implement the required
// CommandStorageManagerDelegate to handle all the persistence of the tab
// restore service implementation.
class TabRestoreServiceImpl::PersistenceDelegate
    : public CommandStorageManagerDelegate,
      public TabRestoreServiceHelper::Observer {
 public:
  explicit PersistenceDelegate(TabRestoreServiceClient* client);

  PersistenceDelegate(const PersistenceDelegate&) = delete;
  PersistenceDelegate& operator=(const PersistenceDelegate&) = delete;

  ~PersistenceDelegate() override;

  // CommandStorageManagerDelegate:
  bool ShouldUseDelayedSave() override;
  void OnWillSaveCommands() override;
  void OnErrorWritingSessionCommands() override;

  // TabRestoreServiceHelper::Observer:
  void OnClearEntries() override;
  void OnNavigationEntriesDeleted() override;
  void OnRestoreEntryById(SessionID id,
                          Entries::const_iterator entry_iterator) override;
  void OnAddEntry() override;

  void set_tab_restore_service_helper(
      TabRestoreServiceHelper* tab_restore_service_helper) {
    tab_restore_service_helper_ = tab_restore_service_helper;
  }

  void LoadTabsFromLastSession();

  void DeleteLastSession();

  bool IsLoaded() const;

  // Creates and add entries to |entries| for each of the windows in |windows|.
  static void CreateEntriesFromWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
      std::vector<std::unique_ptr<tab_restore::Entry>>* entries);

  void Shutdown();

  // Schedules the commands for a window close.
  void ScheduleCommandsForWindow(const tab_restore::Window& window);

  // Schedules the commands for a group close.
  void ScheduleCommandsForGroup(const tab_restore::Group& group);

  // Schedules the commands for a list of tabs (from a window or group).
  void ScheduleCommandsForTabs(
      const std::vector<std::unique_ptr<tab_restore::Tab>>& tabs);

  // Schedules the commands for a tab close. |selected_index| gives the index of
  // the selected navigation.
  void ScheduleCommandsForTab(const tab_restore::Tab& tab, int selected_index);

  void ScheduleRestoredEntryCommandsForTest(SessionID id);

  // Creates a window close command.
  static std::unique_ptr<SessionCommand> CreateWindowCommand(
      SessionID window_id,
      SessionWindow::WindowType type,
      int selected_tab_index,
      int num_tabs,
      const gfx::Rect& bounds,
      ui::mojom::WindowShowState show_state,
      const std::string& workspace,
      base::Time timestamp);

  // Creates a group close command.
  static std::unique_ptr<SessionCommand> CreateGroupCommand(
      SessionID session_id,
      size_t num_tabs,
      tab_groups::TabGroupId group_id,
      std::optional<base::Uuid> saved_group_id,
      SessionID::id_type browser_id,
      tab_groups::TabGroupVisualData visual_data);

  // Creates a tab close command.
  static std::unique_ptr<SessionCommand> CreateSelectedNavigationInTabCommand(
      SessionID tab_id,
      int32_t index,
      base::Time timestamp);

  // Creates a restore command.
  static std::unique_ptr<SessionCommand> CreateRestoredEntryCommand(
      SessionID entry_id);

  // Returns the index to persist as the selected index. This is the same as
  // |tab.current_navigation_index| unless the entry at
  // |tab.current_navigation_index| shouldn't be persisted. Returns -1 if no
  // valid navigation to persist.
  int GetSelectedNavigationIndexToPersist(const tab_restore::Tab& tab);

  // Invoked when we've loaded the session commands that identify the previously
  // closed tabs. This creates entries, adds them to staging_entries_, and
  // invokes LoadState.
  void OnGotLastSessionCommands(
      std::vector<std::unique_ptr<SessionCommand>> commands,
      bool read_error);

  // Populates |loaded_entries| with Entries from |commands|.
  void CreateEntriesFromCommands(
      const std::vector<std::unique_ptr<SessionCommand>>& commands,
      std::vector<std::unique_ptr<tab_restore::Entry>>* loaded_entries);

  // Validates all entries in |entries|, deleting any with no navigations. This
  // also deletes any entries beyond the max number of entries we can hold.
  static void ValidateAndDeleteEmptyEntries(
      std::vector<std::unique_ptr<tab_restore::Entry>>* entries);

  // Callback from CommandStorageManager when we've received the windows from
  // the previous session. This creates and add entries to |staging_entries_|
  // and invokes LoadStateChanged. |ignored_active_window| is ignored because we
  // don't need to restore activation.
  void OnGotPreviousSession(std::vector<std::unique_ptr<SessionWindow>> windows,
                            SessionID ignored_active_window,
                            bool error_reading);

  // Converts a SessionWindow into a Window, returning true on success. We use 0
  // as the timestamp here since we do not know when the window/tab was closed.
  static bool ConvertSessionWindowToWindow(SessionWindow* session_window,
                                           tab_restore::Window* window);

  // Invoked when previous tabs or session is loaded. If both have finished
  // loading the entries in |staging_entries_| are added to entries and
  // observers are notified.
  void LoadStateChanged();

 private:
  // The associated client.
  raw_ptr<TabRestoreServiceClient> client_;

  std::unique_ptr<CommandStorageManager> command_storage_manager_;

  raw_ptr<TabRestoreServiceHelper> tab_restore_service_helper_;

  // The number of entries to write.
  int entries_to_write_;

  // Number of entries we've written.
  int entries_written_;

  // Whether we've loaded the last session.
  int load_state_;

  // Results from previously closed tabs/sessions is first added here. When the
  // results from both us and the session restore service have finished loading
  // LoadStateChanged is invoked, which adds these entries to entries_.
  std::vector<std::unique_ptr<tab_restore::Entry>> staging_entries_;

  // Used when loading previous tabs/session and open tabs/session.
  base::WeakPtrFactory<PersistenceDelegate> weak_factory_{this};
};

TabRestoreServiceImpl::PersistenceDelegate::PersistenceDelegate(
    TabRestoreServiceClient* client)
    : client_(client),
      command_storage_manager_(std::make_unique<CommandStorageManager>(
          CommandStorageManager::kTabRestore,
          client_->GetPathToSaveTo(),
          this)),
      tab_restore_service_helper_(nullptr),
      entries_to_write_(0),
      entries_written_(0),
      load_state_(NOT_LOADED) {}

TabRestoreServiceImpl::PersistenceDelegate::~PersistenceDelegate() = default;

bool TabRestoreServiceImpl::PersistenceDelegate::ShouldUseDelayedSave() {
  return true;
}

void TabRestoreServiceImpl::PersistenceDelegate::OnWillSaveCommands() {
  const Entries& entries = tab_restore_service_helper_->entries();
  int to_write_count =
      std::min(entries_to_write_, static_cast<int>(entries.size()));
  entries_to_write_ = 0;
  if (entries_written_ + to_write_count > kEntriesPerReset ||
      command_storage_manager_->pending_reset()) {
    to_write_count = entries.size();
    command_storage_manager_->set_pending_reset(true);
  }
  if (to_write_count) {
    // Write the to_write_count most recently added entries out. The most
    // recently added entry is at the front, so we use a reverse iterator to
    // write in the order the entries were added.
    auto i = entries.rbegin();
    DCHECK(static_cast<size_t>(to_write_count) <= entries.size());
    std::advance(i, entries.size() - static_cast<int>(to_write_count));
    for (; i != entries.rend(); ++i) {
      tab_restore::Entry& entry = **i;
      switch (entry.type) {
        case tab_restore::Type::TAB: {
          tab_restore::Tab& tab = static_cast<tab_restore::Tab&>(entry);
          int selected_index = GetSelectedNavigationIndexToPersist(tab);
          if (selected_index != -1) {
            ScheduleCommandsForTab(tab, selected_index);
          }
          break;
        }
        case tab_restore::Type::WINDOW:
          ScheduleCommandsForWindow(static_cast<tab_restore::Window&>(entry));
          break;
        case tab_restore::Type::GROUP:
          ScheduleCommandsForGroup(static_cast<tab_restore::Group&>(entry));
          break;
      }
      entries_written_++;
    }
  }
  if (command_storage_manager_->pending_reset()) {
    entries_written_ = 0;
  }
}

void TabRestoreServiceImpl::PersistenceDelegate::
    OnErrorWritingSessionCommands() {
  command_storage_manager_->set_pending_reset(true);
  command_storage_manager_->StartSaveTimer();
}

void TabRestoreServiceImpl::PersistenceDelegate::OnClearEntries() {
  // Mark all the tabs as closed so that we don't attempt to restore them.
  const Entries& entries = tab_restore_service_helper_->entries();
  for (const auto& entry : entries) {
    command_storage_manager_->ScheduleCommand(
        CreateRestoredEntryCommand(entry->id));
  }

  entries_to_write_ = 0;

  // Schedule a pending reset so that we nuke the file on next write.
  command_storage_manager_->set_pending_reset(true);
  // Schedule a command, otherwise if there are no pending commands Save does
  // nothing.
  command_storage_manager_->ScheduleCommand(
      CreateRestoredEntryCommand(SessionID::InvalidValue()));
}

void TabRestoreServiceImpl::PersistenceDelegate::OnNavigationEntriesDeleted() {
  // Rewrite all entries.
  entries_to_write_ = tab_restore_service_helper_->entries().size();

  // Schedule a pending reset so that we nuke the file on next write.
  command_storage_manager_->set_pending_reset(true);
  // Schedule a command, otherwise if there are no pending commands Save does
  // nothing.
  command_storage_manager_->ScheduleCommand(
      CreateRestoredEntryCommand(SessionID::InvalidValue()));
}

void TabRestoreServiceImpl::PersistenceDelegate::OnRestoreEntryById(
    SessionID id,
    Entries::const_iterator entry_iterator) {
  size_t index = 0;
  const Entries& entries = tab_restore_service_helper_->entries();
  for (auto j = entries.begin(); j != entry_iterator && j != entries.end();
       ++j, ++index) {
  }
  if (static_cast<int>(index) < entries_to_write_) {
    entries_to_write_--;
  }
  command_storage_manager_->ScheduleCommand(CreateRestoredEntryCommand(id));
}

void TabRestoreServiceImpl::PersistenceDelegate::OnAddEntry() {
  // Start the save timer, when it fires we'll generate the commands.
  command_storage_manager_->StartSaveTimer();
  entries_to_write_++;
}

void TabRestoreServiceImpl::PersistenceDelegate::LoadTabsFromLastSession() {
  if (load_state_ != NOT_LOADED) {
    return;
  }
  if (tab_restore_service_helper_->entries().size() == kMaxEntries) {
    // We already have the max number of entries we can take. There is no point
    // in attempting to load since we'll just drop the results. Skip to loaded.
    load_state_ = (LOADING | LOADED_LAST_SESSION | LOADED_LAST_TABS);
    LoadStateChanged();
    return;
  }

  load_state_ = LOADING;
  if (client_->HasLastSession()) {
    client_->GetLastSession(
        base::BindOnce(&PersistenceDelegate::OnGotPreviousSession,
                       weak_factory_.GetWeakPtr()));
  } else {
    load_state_ |= LOADED_LAST_SESSION;
  }

  // Request the tabs closed in the last session. If the last session crashed,
  // this won't contain the tabs/window that were open at the point of the
  // crash (the call to GetLastSession above requests those).
  command_storage_manager_->GetLastSessionCommands(
      base::BindOnce(&PersistenceDelegate::OnGotLastSessionCommands,
                     weak_factory_.GetWeakPtr()));
}

void TabRestoreServiceImpl::PersistenceDelegate::DeleteLastSession() {
  command_storage_manager_->DeleteLastSession();
}

bool TabRestoreServiceImpl::PersistenceDelegate::IsLoaded() const {
  return !(load_state_ & (NOT_LOADED | LOADING));
}

// static
void TabRestoreServiceImpl::PersistenceDelegate::CreateEntriesFromWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
    std::vector<std::unique_ptr<tab_restore::Entry>>* entries) {
  for (const auto& session_window : *windows) {
    std::unique_ptr<tab_restore::Window> window =
        std::make_unique<tab_restore::Window>();
    if (ConvertSessionWindowToWindow(session_window.get(), window.get())) {
      entries->push_back(std::move(window));
    }
  }
}

void TabRestoreServiceImpl::PersistenceDelegate::Shutdown() {
  command_storage_manager_->Save();
}

void TabRestoreServiceImpl::PersistenceDelegate::ScheduleCommandsForWindow(
    const tab_restore::Window& window) {
  DCHECK(!window.tabs.empty());
  int selected_tab = window.selected_tab_index;
  int valid_tab_count = 0;
  int real_selected_tab = selected_tab;
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    if (GetSelectedNavigationIndexToPersist(*window.tabs[i]) != -1) {
      valid_tab_count++;
    } else if (static_cast<int>(i) < selected_tab) {
      real_selected_tab--;
    }
  }
  if (valid_tab_count == 0) {
    return;  // No tabs to persist.
  }
  command_storage_manager_->ScheduleCommand(CreateWindowCommand(
      window.id, window.type, std::min(real_selected_tab, valid_tab_count - 1),
      valid_tab_count, window.bounds, window.show_state, window.workspace,
      window.timestamp));

  if (!window.app_name.empty()) {
    command_storage_manager_->ScheduleCommand(CreateSetWindowAppNameCommand(
        kCommandSetWindowAppName, window.id, window.app_name));
  }

  if (!window.user_title.empty()) {
    command_storage_manager_->ScheduleCommand(CreateSetWindowUserTitleCommand(
        kCommandSetWindowUserTitle, window.id, window.user_title));
  }

  ScheduleCommandsForTabs(window.tabs);
}

void TabRestoreServiceImpl::PersistenceDelegate::ScheduleCommandsForGroup(
    const tab_restore::Group& group) {
  DCHECK(!group.tabs.empty());

  command_storage_manager_->ScheduleCommand(CreateGroupCommand(
      group.id, group.tabs.size(), group.group_id, group.saved_group_id,
      group.browser_id, group.visual_data));
  ScheduleCommandsForTabs(group.tabs);
}

void TabRestoreServiceImpl::PersistenceDelegate::ScheduleCommandsForTabs(
    const std::vector<std::unique_ptr<tab_restore::Tab>>& tabs) {
  for (const std::unique_ptr<tab_restore::Tab>& tab : tabs) {
    int selected_index = GetSelectedNavigationIndexToPersist(*tab);
    if (selected_index != -1) {
      ScheduleCommandsForTab(*tab, selected_index);
    }
  }
}

void TabRestoreServiceImpl::PersistenceDelegate::ScheduleCommandsForTab(
    const tab_restore::Tab& tab,
    int selected_index) {
  const std::vector<SerializedNavigationEntry>& navigations = tab.navigations;
  int max_index = static_cast<int>(navigations.size());

  // Determine the first navigation we'll persist.
  int valid_count_before_selected = 0;
  int first_index_to_persist = selected_index;
  for (int i = selected_index - 1;
       i >= 0 && valid_count_before_selected < gMaxPersistNavigationCount;
       --i) {
    if (client_->ShouldTrackURLForRestore(navigations[i].virtual_url())) {
      first_index_to_persist = i;
      valid_count_before_selected++;
    }
  }

  // Write the command that identifies the selected tab.
  command_storage_manager_->ScheduleCommand(
      CreateSelectedNavigationInTabCommand(tab.id, valid_count_before_selected,
                                           tab.timestamp));

  if (tab.pinned) {
    PinnedStatePayload payload = true;
    std::unique_ptr<SessionCommand> command(
        new SessionCommand(kCommandPinnedState, sizeof(payload)));
    memcpy(command->contents(), &payload, sizeof(payload));
    command_storage_manager_->ScheduleCommand(std::move(command));
  }

  if (tab.group.has_value()) {
    base::Pickle pickle;
    WriteTokenToPickle(&pickle, tab.group.value().token());
    const tab_groups::TabGroupVisualData* visual_data =
        &tab.group_visual_data.value();
    pickle.WriteString16(visual_data->title());
    pickle.WriteUInt32(static_cast<int>(visual_data->color()));

    // Added in M126. Write the saved group id to the pickle if there is one.
    pickle.WriteBool(tab.saved_group_id.has_value());
    if (tab.saved_group_id.has_value()) {
      pickle.WriteString(tab.saved_group_id.value().AsLowercaseString());
    }

    std::unique_ptr<SessionCommand> command(
        new SessionCommand(kCommandSetTabGroupData, pickle));
    command_storage_manager_->ScheduleCommand(std::move(command));
  }

  if (!tab.extension_app_id.empty()) {
    command_storage_manager_->ScheduleCommand(CreateSetTabExtensionAppIDCommand(
        kCommandSetExtensionAppID, tab.id, tab.extension_app_id));
  }

  if (!tab.user_agent_override.ua_string_override.empty()) {
    command_storage_manager_->ScheduleCommand(
        CreateSetTabUserAgentOverrideCommand(kCommandSetTabUserAgentOverride2,
                                             tab.id, tab.user_agent_override));
  }

  // Then write the navigations.
  for (int i = first_index_to_persist, wrote_count = 0;
       wrote_count < 2 * gMaxPersistNavigationCount && i < max_index; ++i) {
    if (client_->ShouldTrackURLForRestore(navigations[i].virtual_url())) {
      command_storage_manager_->ScheduleCommand(
          CreateUpdateTabNavigationCommand(kCommandUpdateTabNavigation, tab.id,
                                           navigations[i]));
    }
  }

  for (const auto& data : tab.extra_data) {
    command_storage_manager_->ScheduleCommand(CreateAddExtraDataCommand(
        kCommandAddTabExtraData, tab.id, data.first, data.second));
  }
}

// static
std::unique_ptr<SessionCommand>
TabRestoreServiceImpl::PersistenceDelegate::CreateWindowCommand(
    SessionID window_id,
    SessionWindow::WindowType type,
    int selected_tab_index,
    int num_tabs,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state,
    const std::string& workspace,
    base::Time timestamp) {
  static_assert(sizeof(SessionID::id_type) == sizeof(int),
                "SessionID::id_type has changed size.");

  // Use a pickle to handle marshaling as this command contains variable-length
  // content.
  base::Pickle pickle;
  pickle.WriteInt(static_cast<int>(window_id.id()));
  pickle.WriteInt(selected_tab_index);
  pickle.WriteInt(num_tabs);
  pickle.WriteInt64(timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  pickle.WriteInt(bounds.x());
  pickle.WriteInt(bounds.y());
  pickle.WriteInt(bounds.width());
  pickle.WriteInt(bounds.height());
  pickle.WriteInt(SerializeWindowShowState(show_state));

  // Enforce a maximum length on workspace names. A common size is 32 bytes for
  // GUIDs.
  if (workspace.size() <= 128) {
    pickle.WriteString(workspace);
  } else {
    pickle.WriteString(std::string());
  }
  pickle.WriteInt(type);

  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandWindow, pickle));
  return command;
}

// static
std::unique_ptr<SessionCommand>
TabRestoreServiceImpl::PersistenceDelegate::CreateGroupCommand(
    SessionID session_id,
    size_t num_tabs,
    tab_groups::TabGroupId tab_group_id,
    std::optional<base::Uuid> saved_group_id,
    SessionID::id_type browser_id,
    tab_groups::TabGroupVisualData visual_data) {
  static_assert(sizeof(SessionID::id_type) == sizeof(int),
                "SessionID::id_type has changed size.");

  base::Pickle pickle;
  WriteTokenToPickle(&pickle, tab_group_id.token());
  pickle.WriteInt(static_cast<int>(session_id.id()));
  pickle.WriteInt(static_cast<int>(num_tabs));
  pickle.WriteInt(static_cast<int>(browser_id));
  pickle.WriteString16(visual_data.title());
  pickle.WriteUInt32(static_cast<int>(visual_data.color()));

  // Added in M126. Write the saved group id to the pickle if there is one.
  pickle.WriteBool(saved_group_id.has_value());
  if (saved_group_id.has_value()) {
    pickle.WriteString(saved_group_id.value().AsLowercaseString());
  }

  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandCreateGroup, pickle));
  return command;
}

// static
std::unique_ptr<SessionCommand> TabRestoreServiceImpl::PersistenceDelegate::
    CreateSelectedNavigationInTabCommand(SessionID tab_id,
                                         int32_t index,
                                         base::Time timestamp) {
  SelectedNavigationInTabPayload2 payload;
  payload.id = tab_id.id();
  payload.index = index;
  payload.timestamp = timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds();
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandSelectedNavigationInTab, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

// static
std::unique_ptr<SessionCommand>
TabRestoreServiceImpl::PersistenceDelegate::CreateRestoredEntryCommand(
    SessionID entry_id) {
  RestoredEntryPayload payload = entry_id.id();
  std::unique_ptr<SessionCommand> command(
      new SessionCommand(kCommandRestoredEntry, sizeof(payload)));
  memcpy(command->contents(), &payload, sizeof(payload));
  return command;
}

int TabRestoreServiceImpl::PersistenceDelegate::
    GetSelectedNavigationIndexToPersist(const tab_restore::Tab& tab) {
  const std::vector<SerializedNavigationEntry>& navigations = tab.navigations;
  int selected_index = tab.current_navigation_index;
  int max_index = static_cast<int>(navigations.size());

  // Find the first navigation to persist. We won't persist the selected
  // navigation if client_->ShouldTrackURLForRestore returns false.
  while (selected_index >= 0 &&
         !client_->ShouldTrackURLForRestore(
             navigations[selected_index].virtual_url())) {
    selected_index--;
  }

  if (selected_index != -1) {
    return selected_index;
  }
  // Couldn't find a navigation to persist going back, go forward.
  selected_index = tab.current_navigation_index + 1;
  while (selected_index < max_index &&
         !client_->ShouldTrackURLForRestore(
             navigations[selected_index].virtual_url())) {
    selected_index++;
  }

  return (selected_index == max_index) ? -1 : selected_index;
}

void TabRestoreServiceImpl::PersistenceDelegate::OnGotLastSessionCommands(
    std::vector<std::unique_ptr<SessionCommand>> commands,
    bool read_error) {
  std::vector<std::unique_ptr<tab_restore::Entry>> entries;
  CreateEntriesFromCommands(commands, &entries);
  // Closed tabs always go to the end.
  staging_entries_.insert(staging_entries_.end(),
                          make_move_iterator(entries.begin()),
                          make_move_iterator(entries.end()));
  load_state_ |= LOADED_LAST_TABS;
  LoadStateChanged();
}

void TabRestoreServiceImpl::PersistenceDelegate::CreateEntriesFromCommands(
    const std::vector<std::unique_ptr<SessionCommand>>& commands,
    std::vector<std::unique_ptr<tab_restore::Entry>>* loaded_entries) {
  if (tab_restore_service_helper_->entries().size() == kMaxEntries) {
    return;
  }
  // Iterate through the commands, populating |entries|.
  std::vector<std::unique_ptr<tab_restore::Entry>> entries;
  // If non-null we're processing the navigations of this tab.
  tab_restore::Tab* current_tab = nullptr;
  // If non-null we're processing the tabs of this window. The int represents
  // the number of tabs left to process within the window.
  std::optional<std::pair<tab_restore::Window*, int>> current_window;
  // If non-null we're processing the tabs of this group. The int represents
  // the number of tabs left to process within the group.
  std::optional<std::pair<tab_restore::Group*, int>> current_group;
  for (const auto& i : commands) {
    const SessionCommand& command = *i;
    switch (command.id()) {
      case kCommandRestoredEntry: {
        if (current_window.has_value() || current_group.has_value()) {
          // Should never receive a restored command while waiting for all the
          // tabs in a window or group.
          return;
        }

        current_tab = nullptr;
        current_window = std::nullopt;
        current_group = std::nullopt;

        RestoredEntryPayload payload;
        if (!command.GetPayload(&payload, sizeof(payload))) {
          return;
        }
        RemoveEntryByID(SessionID::FromSerializedValue(payload), &entries);
        break;
      }

      case kCommandWindowDeprecated:
      case kCommandWindow: {
        // Should never receive a window command while waiting for all the
        // tabs in a window or group.
        if (current_window.has_value() || current_group.has_value()) {
          return;
        }

        int32_t num_tabs = 0;
        SessionID window_id = SessionID::InvalidValue();
        std::unique_ptr<tab_restore::Window> window =
            CreateWindowEntryFromCommand(&command, &window_id, &num_tabs);
        if (!window) {
          return;
        }

        // Should always have at least 1 tab. Likely indicates corruption.
        if (num_tabs <= 0) {
          return;
        }

        RemoveEntryByID(window_id, &entries);
        window->original_id = window_id;
        current_window =
            std::make_optional(std::make_pair(window.get(), num_tabs));
        entries.push_back(std::move(window));
        break;
      }
      case kCommandCreateGroup: {
        // Should never receive a group command while waiting for all the
        // tabs in a window or group.
        if (current_window.has_value() || current_group.has_value()) {
          return;
        }

        int32_t num_tabs = 0;
        SessionID group_id = SessionID::InvalidValue();
        std::unique_ptr<tab_restore::Group> group =
            CreateGroupEntryFromCommand(&command, &group_id, &num_tabs);
        if (!group) {
          return;
        }

        // Should always have at least 1 tab. Likely indicates corruption.
        if (num_tabs <= 0) {
          return;
        }

        RemoveEntryByID(group_id, &entries);
        group->original_id = group_id;
        current_group =
            std::make_optional(std::make_pair(group.get(), num_tabs));
        entries.push_back(std::move(group));
        break;
      }
      case kCommandSelectedNavigationInTab: {
        SelectedNavigationInTabPayload2 payload;
        if (!command.GetPayload(&payload, sizeof(payload))) {
          SelectedNavigationInTabPayload old_payload;
          if (!command.GetPayload(&old_payload, sizeof(old_payload))) {
            return;
          }
          payload.id = old_payload.id;
          payload.index = old_payload.index;
          // Since we don't have a time use time 0 which is used to mark as an
          // unknown timestamp.
          payload.timestamp = 0;
        }

        if (current_window.has_value()) {
          DCHECK_EQ(current_group.has_value(), false);
          if (!current_window->first) {
            // We should have created a window already.
            NOTREACHED_IN_MIGRATION();
            return;
          }
          current_window->first->tabs.push_back(
              std::make_unique<tab_restore::Tab>());
          current_tab = current_window->first->tabs.back().get();
          if (--current_window->second == 0) {
            current_window = std::nullopt;
          }
        } else if (current_group.has_value()) {
          if (!current_group->first) {
            // We should have created a group already.
            NOTREACHED_IN_MIGRATION();
            return;
          }
          current_group->first->tabs.push_back(
              std::make_unique<tab_restore::Tab>());
          current_tab = current_group->first->tabs.back().get();
          if (--current_group->second == 0) {
            current_group = std::nullopt;
          }
        } else {
          RemoveEntryByID(SessionID::FromSerializedValue(payload.id), &entries);
          entries.push_back(std::make_unique<tab_restore::Tab>());
          current_tab = static_cast<tab_restore::Tab*>(entries.back().get());
          current_tab->timestamp = base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(payload.timestamp));
        }
        current_tab->current_navigation_index = payload.index;
        current_tab->original_id = SessionID::FromSerializedValue(payload.id);
        break;
      }

      case kCommandUpdateTabNavigation: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        current_tab->navigations.resize(current_tab->navigations.size() + 1);
        SessionID tab_id = SessionID::InvalidValue();
        if (!RestoreUpdateTabNavigationCommand(
                command, &current_tab->navigations.back(), &tab_id)) {
          return;
        }
        // When navigations are serialized, only gMaxPersistNavigationCount
        // navigations are written. This leads to inconsistent indices.
        current_tab->navigations.back().set_index(
            current_tab->navigations.size() - 1);
        break;
      }

      case kCommandPinnedState: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        // NOTE: payload doesn't matter. kCommandPinnedState is only written if
        // tab is pinned.
        current_tab->pinned = true;
        break;
      }

      case kCommandSetTabGroupData: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        base::Pickle pickle = command.PayloadAsPickle();
        base::PickleIterator iter(pickle);
        std::optional<base::Token> group_token = ReadTokenFromPickle(&iter);
        std::u16string title;
        uint32_t color_int;
        bool is_saved;
        std::string saved_id;
        if (!iter.ReadString16(&title) || !iter.ReadUInt32(&color_int)) {
          break;
        }
        if (iter.ReadBool(&is_saved) && is_saved) {
          if (!iter.ReadString(&saved_id) || saved_id.empty()) {
            break;
          }
          current_tab->saved_group_id = base::Uuid::ParseLowercase(saved_id);
        } else {
          // Explicitly update the nullopt if the group isn't saved. This is to
          // ensure the right value is set when there are multiple entries in
          // the append log file.
          current_tab->saved_group_id = std::nullopt;
        }

        current_tab->group =
            tab_groups::TabGroupId::FromRawToken(group_token.value());

        current_tab->group_visual_data =
            tab_groups::TabGroupVisualData(title, color_int);
        break;
      }

      case kCommandSetWindowAppName: {
        if (!current_window->first) {
          // We should have created a window already.
          NOTREACHED_IN_MIGRATION();
          return;
        }

        SessionID window_id = SessionID::InvalidValue();
        std::string app_name;
        if (!RestoreSetWindowAppNameCommand(command, &window_id, &app_name)) {
          return;
        }
        current_window->first->app_name.swap(app_name);
        break;
      }

      case kCommandSetExtensionAppID: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        SessionID tab_id = SessionID::InvalidValue();
        std::string extension_app_id;
        if (!RestoreSetTabExtensionAppIDCommand(command, &tab_id,
                                                &extension_app_id)) {
          return;
        }
        current_tab->extension_app_id.swap(extension_app_id);
        break;
      }

      case kCommandSetTabUserAgentOverride: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        SessionID tab_id = SessionID::InvalidValue();
        std::string user_agent_override;
        if (!RestoreSetTabUserAgentOverrideCommand(command, &tab_id,
                                                   &user_agent_override)) {
          return;
        }
        current_tab->user_agent_override.ua_string_override.swap(
            user_agent_override);
        current_tab->user_agent_override.opaque_ua_metadata_override =
            std::nullopt;
        break;
      }

      case kCommandSetTabUserAgentOverride2: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        SessionID tab_id = SessionID::InvalidValue();
        std::string user_agent_override;
        std::optional<std::string> opaque_ua_metadata_override;
        if (!RestoreSetTabUserAgentOverrideCommand2(
                command, &tab_id, &user_agent_override,
                &opaque_ua_metadata_override)) {
          return;
        }
        current_tab->user_agent_override.ua_string_override =
            std::move(user_agent_override);
        current_tab->user_agent_override.opaque_ua_metadata_override =
            std::move(opaque_ua_metadata_override);
        break;
      }

      case kCommandSetWindowUserTitle: {
        if (!current_window->first) {
          // We should have created a window already.
          NOTREACHED_IN_MIGRATION();
          return;
        }

        SessionID window_id = SessionID::InvalidValue();
        std::string title;
        if (!RestoreSetWindowUserTitleCommand(command, &window_id, &title)) {
          return;
        }

        current_window->first->user_title.swap(title);
        break;
      }

      case kCommandAddTabExtraData: {
        if (!current_tab) {
          // Should be in a tab when we get this.
          return;
        }
        SessionID tab_id = SessionID::InvalidValue();
        std::string key;
        std::string data;
        if (!RestoreAddExtraDataCommand(command, &tab_id, &key, &data)) {
          return;
        }
        current_tab->extra_data[key] = std::move(data);
        break;
      }

      default:
        // Unknown type, usually indicates corruption of file. Ignore it.
        return;
    }
  }

  // If there was corruption some of the entries won't be valid.
  ValidateAndDeleteEmptyEntries(&entries);
  loaded_entries->swap(entries);
}

// static
void TabRestoreServiceImpl::PersistenceDelegate::ValidateAndDeleteEmptyEntries(
    std::vector<std::unique_ptr<tab_restore::Entry>>* entries) {
  std::vector<std::unique_ptr<tab_restore::Entry>> valid_entries;

  // Iterate from the back so that we keep the most recently closed entries.
  for (std::unique_ptr<tab_restore::Entry>& entry : base::Reversed(*entries)) {
    if (TabRestoreServiceHelper::ValidateEntry(*entry)) {
      valid_entries.push_back(std::move(entry));
    }
  }
  // NOTE: at this point the entries are ordered with newest at the front.
  entries->swap(valid_entries);
}

void TabRestoreServiceImpl::PersistenceDelegate::OnGotPreviousSession(
    std::vector<std::unique_ptr<SessionWindow>> windows,
    SessionID ignored_active_window,
    bool error_reading) {
  std::vector<std::unique_ptr<tab_restore::Entry>> entries;
  CreateEntriesFromWindows(&windows, &entries);
  // Previous session tabs go first.
  staging_entries_.insert(staging_entries_.begin(),
                          make_move_iterator(entries.begin()),
                          make_move_iterator(entries.end()));
  load_state_ |= LOADED_LAST_SESSION;
  LoadStateChanged();
}

bool TabRestoreServiceImpl::PersistenceDelegate::ConvertSessionWindowToWindow(
    SessionWindow* session_window,
    tab_restore::Window* window) {
  window->type = session_window->type;

  // The groups in ` window`. The group visual data must also be explicitly set
  // on grouped tabs.
  std::map<tab_groups::TabGroupId, std::unique_ptr<tab_restore::Group>> groups;
  for (auto& tab_group : session_window->tab_groups) {
    auto group_id = tab_group->id;
    auto group = std::make_unique<sessions::tab_restore::Group>();

    group->group_id = tab_group->id;
    if (tab_group->saved_guid.has_value()) {
      group->saved_group_id =
          base::Uuid::ParseLowercase(tab_group->saved_guid.value());
    }
    group->visual_data = tab_group->visual_data;
    group->browser_id = session_window->window_id.id();
    group->timestamp = base::Time::Now();
    groups[group_id] = std::move(group);
  }

  for (auto& i : session_window->tabs) {
    if (i->navigations.empty()) {
      continue;
    }
    window->tabs.push_back(std::make_unique<tab_restore::Tab>());
    tab_restore::Tab& tab = *window->tabs.back();

    auto group_id = i->group;
    if (group_id.has_value()) {
      tab.group = group_id;
      tab.group_visual_data = groups[group_id.value()]->visual_data;
    }

    tab.pinned = i->pinned;
    tab.navigations.swap(i->navigations);
    tab.current_navigation_index = i->current_navigation_index;
    tab.extension_app_id = i->extension_app_id;
    tab.extra_data = std::move(i->extra_data);
    tab.timestamp = base::Time();
  }

  if (window->tabs.empty()) {
    return false;
  }
  window->tab_groups = std::move(groups);
  window->selected_tab_index =
      std::min(session_window->selected_tab_index,
               static_cast<int>(window->tabs.size() - 1));
  window->extra_data = std::move(session_window->extra_data);
  window->timestamp = base::Time();
  window->bounds = session_window->bounds;
  window->show_state = session_window->show_state;
  window->workspace = session_window->workspace;
  return true;
}

void TabRestoreServiceImpl::PersistenceDelegate::LoadStateChanged() {
  if ((load_state_ & (LOADED_LAST_TABS | LOADED_LAST_SESSION)) !=
      (LOADED_LAST_TABS | LOADED_LAST_SESSION)) {
    // Still waiting on previous session or previous tabs.
    return;
  }

  // We're done loading.
  load_state_ ^= LOADING;

  const Entries& entries = tab_restore_service_helper_->entries();
  if (staging_entries_.empty() || entries.size() >= kMaxEntries) {
    staging_entries_.clear();
    tab_restore_service_helper_->NotifyLoaded();
    return;
  }

  if (staging_entries_.size() + entries.size() > kMaxEntries) {
    // If we add all the staged entries we'll end up with more than
    // kMaxEntries. Delete entries such that we only end up with at most
    // kMaxEntries.
    int surplus = kMaxEntries - entries.size();
    CHECK_LE(0, surplus);
    CHECK_GE(static_cast<int>(staging_entries_.size()), surplus);
    staging_entries_.erase(
        staging_entries_.begin() + (kMaxEntries - entries.size()),
        staging_entries_.end());
  }

  // And add them.
  for (auto& staging_entry : staging_entries_) {
    tab_restore_service_helper_->AddEntry(std::move(staging_entry), false,
                                          false);
  }

  staging_entries_.clear();

  tab_restore_service_helper_->PruneEntries();

  // Write the loaded entries into the current session.
  entries_to_write_ = tab_restore_service_helper_->entries().size();

  tab_restore_service_helper_->NotifyEntriesChanged();

  tab_restore_service_helper_->NotifyLoaded();
}

void TabRestoreServiceImpl::PersistenceDelegate::
    ScheduleRestoredEntryCommandsForTest(SessionID id) {
  command_storage_manager_->ScheduleCommand(CreateRestoredEntryCommand(id));
}

// TabRestoreServiceImpl -------------------------------------------------

TabRestoreServiceImpl::TabRestoreServiceImpl(
    std::unique_ptr<TabRestoreServiceClient> client,
    PrefService* pref_service,
    tab_restore::TimeFactory* time_factory)
    : client_(std::move(client)), helper_(this, client_.get(), time_factory) {
  if (pref_service) {
    pref_change_registrar_.Init(pref_service);
    pref_change_registrar_.Add(
        prefs::kSavingBrowserHistoryDisabled,
        base::BindRepeating(&TabRestoreServiceImpl::UpdatePersistenceDelegate,
                            base::Unretained(this)));
  }
  UpdatePersistenceDelegate();
}

TabRestoreServiceImpl::~TabRestoreServiceImpl() = default;

void TabRestoreServiceImpl::AddObserver(TabRestoreServiceObserver* observer) {
  helper_.AddObserver(observer);
}

void TabRestoreServiceImpl::RemoveObserver(
    TabRestoreServiceObserver* observer) {
  helper_.RemoveObserver(observer);
}

std::optional<SessionID> TabRestoreServiceImpl::CreateHistoricalTab(
    LiveTab* live_tab,
    int index) {
  return helper_.CreateHistoricalTab(live_tab, index);
}

void TabRestoreServiceImpl::BrowserClosing(LiveTabContext* context) {
  helper_.BrowserClosing(context);
}

void TabRestoreServiceImpl::BrowserClosed(LiveTabContext* context) {
  helper_.BrowserClosed(context);
}

void TabRestoreServiceImpl::CreateHistoricalGroup(
    LiveTabContext* context,
    const tab_groups::TabGroupId& id) {
  helper_.CreateHistoricalGroup(context, id);
}

void TabRestoreServiceImpl::GroupClosed(const tab_groups::TabGroupId& group) {
  helper_.GroupClosed(group);
}

void TabRestoreServiceImpl::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {
  helper_.GroupCloseStopped(group);
}

void TabRestoreServiceImpl::ClearEntries() {
  helper_.ClearEntries();
}

void TabRestoreServiceImpl::DeleteNavigationEntries(
    const DeletionPredicate& predicate) {
  DCHECK(IsLoaded());
  helper_.DeleteNavigationEntries(predicate);
}

const TabRestoreService::Entries& TabRestoreServiceImpl::entries() const {
  return helper_.entries();
}

std::vector<LiveTab*> TabRestoreServiceImpl::RestoreMostRecentEntry(
    LiveTabContext* context) {
  return helper_.RestoreMostRecentEntry(context);
}

void TabRestoreServiceImpl::RemoveEntryById(SessionID id) {
  helper_.RemoveEntryById(id);
}

std::vector<LiveTab*> TabRestoreServiceImpl::RestoreEntryById(
    LiveTabContext* context,
    SessionID id,
    WindowOpenDisposition disposition) {
  return helper_.RestoreEntryById(context, id, disposition);
}

bool TabRestoreServiceImpl::IsLoaded() const {
  if (persistence_delegate_) {
    return persistence_delegate_->IsLoaded();
  }
  return true;
}

void TabRestoreServiceImpl::DeleteLastSession() {
  if (persistence_delegate_) {
    persistence_delegate_->DeleteLastSession();
  }
}

bool TabRestoreServiceImpl::IsRestoring() const {
  return helper_.IsRestoring();
}

void TabRestoreServiceImpl::Shutdown() {
  if (persistence_delegate_) {
    persistence_delegate_->Shutdown();
  }
}

void TabRestoreServiceImpl::LoadTabsFromLastSession() {
  if (persistence_delegate_) {
    persistence_delegate_->LoadTabsFromLastSession();
  }
}

void TabRestoreServiceImpl::UpdatePersistenceDelegate() {
  // When a persistence delegate has been created, it must be shut down and
  // deleted if a pref service is available and saving history is disabled.
  if (pref_change_registrar_.prefs() &&
      pref_change_registrar_.prefs()->GetBoolean(
          prefs::kSavingBrowserHistoryDisabled)) {
    if (persistence_delegate_) {
      helper_.SetHelperObserver(nullptr);
      // Make sure we don't leave stale data for the next time the pref is
      // changed back to enable.
      persistence_delegate_->DeleteLastSession();
      persistence_delegate_->Shutdown();
      persistence_delegate_.reset(nullptr);
    } else {
      // In case this is the first time Chrome is launched with saving history
      // disabled, we must make sure to clear the previously saved session.
      PersistenceDelegate persistence_delegate(client_.get());
      persistence_delegate.DeleteLastSession();
    }
  } else if (!persistence_delegate_) {
    // When saving is NOT disabled (or there is no pref service available), and
    // there are no persistence delegate yet, one must be created and
    // initialized.
    persistence_delegate_ =
        std::make_unique<PersistenceDelegate>(client_.get());
    persistence_delegate_->set_tab_restore_service_helper(&helper_);
    helper_.SetHelperObserver(persistence_delegate_.get());
  }
}

TabRestoreService::Entries* TabRestoreServiceImpl::mutable_entries() {
  return &helper_.entries_;
}

void TabRestoreServiceImpl::PruneEntries() {
  helper_.PruneEntries();
}

void TabRestoreServiceImpl::CreateRestoredEntryCommandForTest(SessionID id) {
  if (persistence_delegate_) {
    persistence_delegate_->ScheduleRestoredEntryCommandsForTest(id);
  }
}

}  // namespace sessions
