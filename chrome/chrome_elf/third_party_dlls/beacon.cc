// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/beacon.h"

#include "chrome/chrome_elf/blocklist_constants.h"
#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_util.h"

namespace third_party_dlls {

bool LeaveSetupBeacon() {
  HANDLE key_handle = INVALID_HANDLE_VALUE;

  if (!nt::CreateRegKey(nt::HKCU,
                        install_static::GetRegistryPath()
                            .append(blocklist::kRegistryBeaconKeyName)
                            .c_str(),
                        KEY_QUERY_VALUE | KEY_SET_VALUE, &key_handle)) {
    return false;
  }

  DWORD blocking_state = blocklist::BLOCKLIST_STATE_MAX;
  if (!nt::QueryRegValueDWORD(key_handle, blocklist::kBeaconState,
                              &blocking_state) ||
      blocking_state == blocklist::BLOCKLIST_DISABLED) {
    nt::CloseRegKey(key_handle);
    return false;
  }

  // Handle attempt count.
  // Only return true if BL is enabled and succeeded on previous run.
  bool success = false;
  if (blocking_state == blocklist::BLOCKLIST_ENABLED) {
    // If the blocking was successfully initialized on the previous run, reset
    // the failure counter. Then update the beacon state.
    if (nt::SetRegValueDWORD(key_handle, blocklist::kBeaconAttemptCount,
                             static_cast<DWORD>(0))) {
      if (nt::SetRegValueDWORD(key_handle, blocklist::kBeaconState,
                               blocklist::BLOCKLIST_SETUP_RUNNING))
        success = true;
    }
  } else {
    // Some part of the blocking setup failed last time. If this has occurred
    // blocklist::kBeaconMaxAttempts times in a row, we switch the state to
    // failed and skip setting up the blocking.
    DWORD attempt_count = 0;

    nt::QueryRegValueDWORD(key_handle, blocklist::kBeaconAttemptCount,
                           &attempt_count);
    ++attempt_count;
    nt::SetRegValueDWORD(key_handle, blocklist::kBeaconAttemptCount,
                         attempt_count);

    if (attempt_count >= blocklist::kBeaconMaxAttempts) {
      blocking_state = blocklist::BLOCKLIST_SETUP_FAILED;
      nt::SetRegValueDWORD(key_handle, blocklist::kBeaconState, blocking_state);
    }
  }

  nt::CloseRegKey(key_handle);
  return success;
}

bool ResetBeacon() {
  HANDLE key_handle = INVALID_HANDLE_VALUE;

  if (!nt::CreateRegKey(nt::HKCU,
                        install_static::GetRegistryPath()
                            .append(blocklist::kRegistryBeaconKeyName)
                            .c_str(),
                        KEY_QUERY_VALUE | KEY_SET_VALUE, &key_handle)) {
    return false;
  }

  DWORD blocking_state = blocklist::BLOCKLIST_STATE_MAX;
  if (!nt::QueryRegValueDWORD(key_handle, blocklist::kBeaconState,
                              &blocking_state)) {
    nt::CloseRegKey(key_handle);
    return false;
  }

  // Reaching this point with the setup running state means the setup did not
  // crash, so we reset to enabled.  Any other state indicates that setup was
  // skipped; in that case we leave the state alone for later recording.
  if (blocking_state == blocklist::BLOCKLIST_SETUP_RUNNING) {
    if (!nt::SetRegValueDWORD(key_handle, blocklist::kBeaconState,
                              blocklist::BLOCKLIST_ENABLED)) {
      nt::CloseRegKey(key_handle);
      return false;
    }
  }

  nt::CloseRegKey(key_handle);
  return true;
}

}  // namespace third_party_dlls
