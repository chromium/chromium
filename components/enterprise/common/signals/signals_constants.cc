// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/common/signals/signals_constants.h"

namespace enterprise_signals {
namespace names {

// Name of the parameterized signal for getting information from resources
// stored on the file system. This includes the presence/absence of
// files/folders, and also additional signals' extraction from executables.
const char kFileSystemInfo[] = "fileSystemInfo";

// Name of the parameterized signal for getting information from settings
// storage (e.g. Registry, Plist) on the device.
const char kSettings[] = "settings";

// Name of the signal for getting information about AV software installed on
// the device.
const char kAntiVirusInfo[] = "antiVirusInfo";

// Name of the signal for getting information about installed hotfixes on the
// device.
const char kInstalledHotfixes[] = "hotfixes";

}  // namespace names
}  // namespace enterprise_signals
