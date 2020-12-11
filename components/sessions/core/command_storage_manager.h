// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_H_
#define COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/sessions/core/sessions_export.h"

namespace base {
class SequencedTaskRunner;
}

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
  using GetCommandsCallback =
      base::OnceCallback<void(std::vector<std::unique_ptr<SessionCommand>>)>;

  // Creates a new CommandStorageManager. After creation you need to invoke
  // Init. |delegate| will remain owned by the creator and it is guaranteed
  // that its lifetime surpasses this class. |path| is the path to save files
  // to. If |enable_crypto| is true, the contents of the file are encrypted.
  CommandStorageManager(const base::FilePath& path,
                        CommandStorageManagerDelegate* delegate,
                        bool enable_crypto = false);
  CommandStorageManager(const CommandStorageManager&) = delete;
  CommandStorageManager& operator=(const CommandStorageManager&) = delete;
  virtual ~CommandStorageManager();

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

  // Requests the commands for the current session. If |decryption_key| is
  // non-empty it is used to decrypt the contents of the file.
  // WARNING: |callback| may be called after |this| is deleted. In other words,
  // be sure to use a WeakPtr with |callback|.
  void GetCurrentSessionCommands(GetCommandsCallback callback,
                                 const std::vector<uint8_t>& decryption_key);

 protected:
  // Provided for subclasses.
  CommandStorageManager(scoped_refptr<CommandStorageBackend> backend,
                        CommandStorageManagerDelegate* delegate);

  // Creates a SequencedTaskRunner suitable for the backend.
  static scoped_refptr<base::SequencedTaskRunner>
  CreateDefaultBackendTaskRunner();

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner() {
    return backend_task_runner_;
  }

  CommandStorageBackend* backend() { return backend_.get(); }

 private:
  friend class CommandStorageManagerTestHelper;

  // The backend object which reads and saves commands.
  scoped_refptr<CommandStorageBackend> backend_;

  // If true, all commands are encrypted.
  bool use_crypto_ = false;

  // Commands we need to send over to the backend.
  std::vector<std::unique_ptr<SessionCommand>> pending_commands_;

  // Whether the backend file should be recreated the next time we send
  // over the commands.
  bool pending_reset_ = false;

  // The number of commands sent to the backend before doing a reset.
  int commands_since_reset_ = 0;

  CommandStorageManagerDelegate* delegate_;

  // TaskRunner all backend tasks are run on. This is a SequencedTaskRunner as
  // all tasks *must* be processed in the order they are scheduled.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Used solely for saving after a delay, and not to be used for any other
  // purposes.
  base::WeakPtrFactory<CommandStorageManager> weak_factory_for_timer_{this};
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_H_
