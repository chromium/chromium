// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_H_
#define COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sessions/core/sessions_export.h"

namespace sessions {
class CommandStorageManagerDelegate;
class SessionCommand;
class CommandStorageBackend;

// CommandStorageManager is responsible for reading/writing SessionCommands
// to disk. SessionCommands are used to save and restore the state of the
// browser. CommandStorageManager runs on the main thread and uses
// CommandStorageBackend (which runs on a background task runner) for the actual
// reading/writing. In hopes of minimizing IO, SessionCommands are queued up
// and processed after a delay.
class SESSIONS_EXPORT CommandStorageManager {
 public:
  // The bool parameter indicates whether there was an error reading the file.
  // If there was an error, the vector contains the set of commands up to the
  // error.
  using GetCommandsCallback =
      base::OnceCallback<void(std::vector<std::unique_ptr<SessionCommand>>,
                              bool)>;

  // Identifies the type of session service this is. This is used by the
  // backend to determine the name of the files.
  // TODO(sky): this enum is purely for legacy reasons, and should be replaced
  // with consumers building the path (similar to weblayer). Remove in
  // approximately a year (1/2022), when we shouldn't need to worry too much
  // about migrating older data.
  enum SessionType { kAppRestore, kSessionRestore, kTabRestore, kOther };

  // Creates a new CommandStorageManager. After creation you need to invoke
  // Init(). `delegate` is not owned by this and must outlive this. If
  // `enable_crypto` is true, the contents of the file are encrypted.
  //
  // The meaning of `path` depends upon the type. If `type` is `kOther`, then
  // the path is a file name to which `_TIMESTAMP` is added. If `type` is not
  // `kOther`, then it is a path to a directory. The actual file name used
  // depends upon the type. Once SessionType can be removed, this logic can
  // standardize on that of `kOther`.
  CommandStorageManager(
      SessionType type,
      const base::FilePath& path,
      CommandStorageManagerDelegate* delegate,
      bool enable_crypto = false,
      const std::vector<uint8_t>& decryption_key = {},
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner = nullptr);
  CommandStorageManager(const CommandStorageManager&) = delete;
  CommandStorageManager& operator=(const CommandStorageManager&) = delete;
  virtual ~CommandStorageManager();

  static scoped_refptr<base::SequencedTaskRunner>
  CreateDefaultBackendTaskRunner();

  // Helper to generate a new key.
  static std::vector<uint8_t> CreateCryptoKey();

  // Returns the set of commands which were scheduled to be written. Once
  // committed to the backend, the commands are removed from here.
  const std::vector<std::unique_ptr<SessionCommand>>& pending_commands() {
    return pending_commands_;
  }

  // Whether the next save resets the file before writing to it.
  void set_pending_reset(bool value) { pending_reset_ = value; }
  bool pending_reset() const { return pending_reset_; }

  // Returns the number of commands sent down since the last reset.
  int commands_since_reset() const { return commands_since_reset_; }

  // Schedules a command. This adds |command| to pending_commands_ and
  // invokes StartSaveTimer to start a timer that invokes Save at a later
  // time.
  void ScheduleCommand(std::unique_ptr<SessionCommand> command);

  // Appends a command as part of a general rebuild. This will neither count
  // against a rebuild, nor will it trigger a save of commands.
  void AppendRebuildCommand(std::unique_ptr<SessionCommand> command);
  void AppendRebuildCommands(
      std::vector<std::unique_ptr<SessionCommand>> commands);

  // Erase the |old_command| from the list of commands.
  // The passed command will automatically be deleted.
  void EraseCommand(SessionCommand* old_command);

  // Swap a |new_command| into the list of queued commands at the location of
  // the |old_command|. The |old_command| will be automatically deleted in the
  // process.
  void SwapCommand(SessionCommand* old_command,
                   std::unique_ptr<SessionCommand> new_command);

  // Clears all commands from the list.
  void ClearPendingCommands();

  // Starts the timer that invokes Save (if timer isn't already running).
  void StartSaveTimer();

  // Passes all pending commands to the backend for saving.
  void Save();

  // Returns true if StartSaveTimer() has been called, but a save has not yet
  // occurred.
  bool HasPendingSave() const;

  // Moves the current session to the last session.
  void MoveCurrentSessionToLastSession();

  // Deletes the last session.
  void DeleteLastSession();

  // Uses the backend to load the last session commands from disk. |callback|
  // is called once the data has arrived, and may be called after this is
  // deleted.
  void GetLastSessionCommands(GetCommandsCallback callback);

 private:
  friend class CommandStorageManagerTestHelper;

  CommandStorageBackend* backend() { return backend_.get(); }

  // Called by the backend if writing to the file failed.
  void OnErrorWritingToFile();

  // The backend object which reads and saves commands.
  scoped_refptr<CommandStorageBackend> backend_;

  // If true, all commands are encrypted.
  const bool use_crypto_;

  // Commands we need to send over to the backend.
  std::vector<std::unique_ptr<SessionCommand>> pending_commands_;

  // Whether the backend file should be recreated the next time we send
  // over the commands.
  bool pending_reset_ = true;

  // The number of commands sent to the backend before doing a reset.
  int commands_since_reset_ = 0;

  raw_ptr<CommandStorageManagerDelegate> delegate_;

  // TaskRunner all backend tasks are run on. This is a SequencedTaskRunner as
  // all tasks *must* be processed in the order they are scheduled.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  base::WeakPtrFactory<CommandStorageManager> weak_factory_{this};

  // Used solely for saving after a delay, and not to be used for any other
  // purposes.
  base::WeakPtrFactory<CommandStorageManager> weak_factory_for_timer_{this};
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_H_
