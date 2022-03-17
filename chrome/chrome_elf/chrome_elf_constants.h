// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the ChromeELF.

#ifndef CHROME_CHROME_ELF_CHROME_ELF_CONSTANTS_H_
#define CHROME_CHROME_ELF_CHROME_ELF_CONSTANTS_H_

#include <windows.h>

namespace blocklist {

// The name of the blocklist beacon registry key.
extern const wchar_t kRegistryBeaconKeyName[];

// The properties for the blocklist beacon.
extern const wchar_t kBeaconVersion[];
extern const wchar_t kBeaconState[];
extern const wchar_t kBeaconAttemptCount[];

// The number of failures that can occur on startup with the beacon enabled
// before we give up and turn off the blocklist.
extern const DWORD kBeaconMaxAttempts;

// The states for the blocklist setup code.
enum BlocklistState {
  BLOCKLIST_DISABLED = 0,
  BLOCKLIST_ENABLED,
  // The blocklist setup code is running. If this is the state at startup, it
  // means the last setup crashed.
  BLOCKLIST_SETUP_RUNNING,
  // If the last setup crashed, we reassign the state to failed.
  BLOCKLIST_SETUP_FAILED,
  // Always keep this at the end.
  BLOCKLIST_STATE_MAX,
};

}  // namespace blocklist

namespace elf_sec {

// The name of the registry key which controls the enablement of
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE for the browser process.
extern const wchar_t kRegBrowserExtensionPointKeyName[];

}  // namespace elf_sec

#endif  // CHROME_CHROME_ELF_CHROME_ELF_CONSTANTS_H_
