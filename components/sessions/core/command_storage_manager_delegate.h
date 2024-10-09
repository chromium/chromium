// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_DELEGATE_H_
#define COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_DELEGATE_H_

#include <stddef.h>

#include <vector>

namespace sessions {

// The CommandStorageManagerDelegate decouples the CommandStorageManager from
// chrome/content dependencies.
class CommandStorageManagerDelegate {
 public:
  CommandStorageManagerDelegate() = default;

  // Returns true if save operations can be performed as a delayed task - which
  // is usually only used by unit tests.
  virtual bool ShouldUseDelayedSave() = 0;

  // Called when commands are about to be written to disc.
  virtual void OnWillSaveCommands() {}

  // Called when a new crypto key has been generated. This is only called if
  // CommandStorageManager was configured to enable encryption.
  virtual void OnGeneratedNewCryptoKey(const std::vector<uint8_t>& key) {}

  // Called if there is an error in writing commands to the file. The
  // expectation is that once this is called, the delegate calls
  // set_pending_reset(true) and adds commands to restore the current state.
  virtual void OnErrorWritingSessionCommands() = 0;

 protected:
  virtual ~CommandStorageManagerDelegate() = default;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_COMMAND_STORAGE_MANAGER_DELEGATE_H_
