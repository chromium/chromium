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

namespace arc {

struct UpgradeParams;

// Enum that describes which native bridge mode is used to run arm binaries on
// x86.
enum class ArcBinaryTranslationType {
  NONE,
  HOUDINI,
  NDK_TRANSLATION,
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

// Generates a list of props from |upgrade_params|, each of which takes the form
// "prefix.prop_name=value"
std::vector<std::string> GenerateUpgradeProps(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix);

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_
