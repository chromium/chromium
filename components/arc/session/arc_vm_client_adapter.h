// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_
#define COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "components/arc/session/arc_client_adapter.h"
#include "components/arc/session/file_system_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
struct SystemMemoryInfoKB;
class TimeDelta;
}

namespace arc {

struct UpgradeParams;

// Enum that describes which native bridge mode is used to run arm binaries on
// x86.
enum class ArcBinaryTranslationType {
  NONE,
  HOUDINI,
  NDK_TRANSLATION,
};

// For better unit-testing.
class ArcVmClientAdapterDelegate {
 public:
  ArcVmClientAdapterDelegate() = default;
  ArcVmClientAdapterDelegate(const ArcVmClientAdapterDelegate&) = delete;
  ArcVmClientAdapterDelegate& operator=(const ArcVmClientAdapterDelegate&) =
      delete;
  virtual ~ArcVmClientAdapterDelegate() = default;
  virtual bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info);
};

// Returns an adapter for arcvm.
std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter();

using FileSystemStatusRewriter =
    base::RepeatingCallback<void(FileSystemStatus*)>;
std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapterForTesting(
    const FileSystemStatusRewriter& rewriter);

// Sets the path of the boot notification server socket for testing.
void SetArcVmBootNotificationServerAddressForTesting(
    const std::string& path,
    base::TimeDelta connect_timeout_limit,
    base::TimeDelta connect_sleep_duration_initial);

// Sets the an FD ConnectToArcVmBootNotificationServer() returns for testing.
void SetArcVmBootNotificationServerFdForTesting(absl::optional<int> fd);

// Generates a list of props from |upgrade_params|, each of which takes the form
// "prefix.prop_name=value"
std::vector<std::string> GenerateUpgradePropsForTesting(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix);

void SetArcVmClientAdapterDelegateForTesting(
    ArcClientAdapter* adapter,
    std::unique_ptr<ArcVmClientAdapterDelegate> delegate);

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_
