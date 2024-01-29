// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/annotations.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace crosier {

namespace {

const char kInformationalTestsSwitch[] = "informational_tests";

bool BoardSupportsBluetooth() {
  auto* bluez_dbus_manager = bluez::BluezDBusManager::Get();
  if (!bluez_dbus_manager) {
    return false;
  }

  // Some VM images have bluez but no bluetooth adapters.
  auto* adapter_client = bluez_dbus_manager->GetBluetoothAdapterClient();
  return adapter_client && !adapter_client->GetAdapters().empty();
}

// Returns true if the board is known to support Vulkan compositing.
bool BoardSupportsVulkan() {
  // The full board name may have the form "glimmer-signed-mp-v4keys" and we
  // just want "glimmer".
  std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (board.empty()) {
    LOG(ERROR) << "Unable to determine LSB release board";
    return false;
  }
  // Vulkan compositing is only supported on a few boards, so use an allow
  // list.
  return board[0] == "brya" || board[0] == "volteer" || board[0] == "dedede";
}

// Returns whether a USE flag is present in tast_use_flags.txt. USE flags are
// often used to control which software features a board supports.
bool BoardHasUseFlag(const char* flag) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string use_flags;
  CHECK(base::ReadFileToString(
      base::FilePath("/usr/local/etc/tast_use_flags.txt"), &use_flags));
  return base::Contains(use_flags, flag);
}

}  // namespace

bool ShouldRunInformationalTests() {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(kInformationalTestsSwitch);
}

bool HasRequirement(Requirement r) {
  switch (r) {
    case Requirement::kBluetooth:
      return BoardSupportsBluetooth();
    case Requirement::kOndeviceHandwriting:
      return BoardHasUseFlag("ondevice_handwriting");
    case Requirement::kVulkan:
      return BoardSupportsVulkan();
  }
  return false;
}

}  // namespace crosier
