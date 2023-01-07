// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/last_breaking_installer_version.h"

namespace installer {

// Document any breaking change when updating `kLastBreakingInstallerVersion`
// with the breaking change and the reason why it is a breaking change.
// When updating the documentation, keep a history of maximum 3 milestones prior
// the current version because downgrades are supported for 3 milestones.
//
// Breaking changes from 85.0.4169.0:
//  Change: Default installation directory for fresh 64 bits browsers moved from
//    base::DIR_PROGRAM_FILESX86 to DIR_PROGRAM_FILES.
//  Reason for being breaking: Downgrading to previous version will result in
//    stale data in DIR_PROGRAM_FILES since the previous installers do not know
//    if there is Chrome data at this location.
const wchar_t kLastBreakingInstallerVersion[] = L"85.0.4169.0";

}  // namespace installer
