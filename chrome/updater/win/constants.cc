// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/constants.h"

#include "base/strings/string16.h"

namespace updater {

const base::char16 kGlobalPrefix[] = L"Global\\G";

// TODO(crbug.com/1097297): need to add branding support.
const base::char16 kPrefsAccessMutex[] =
    L"{D8E4A6FE-EA7A-4D20-A8C8-B4628776A101}";

const base::char16 kRegValuePV[] = L"pv";
const base::char16 kRegValueName[] = L"name";

const base::char16 kRegValueInstallerError[] = L"InstallerError";
const base::char16 kRegValueInstallerExtraCode1[] = L"InstallerExtraCode1";
const base::char16 kRegValueInstallerProgress[] = L"InstallerProgress";
const base::char16 kRegValueInstallerResult[] = L"InstallerResult";
const base::char16 kRegValueInstallerResultUIString[] =
    L"InstallerResultUIString";
const base::char16 kRegValueInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

const base::char16 kWindowsServiceName[] = L"UpdaterService";

}  // namespace updater
