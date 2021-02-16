// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/constants.h"

namespace updater {

const wchar_t kGlobalPrefix[] = L"Global\\G";

// TODO(crbug.com/1097297): need to add branding support.
const wchar_t kPrefsAccessMutex[] = L"{D8E4A6FE-EA7A-4D20-A8C8-B4628776A101}";

const wchar_t kRegValuePV[] = L"pv";
const wchar_t kRegValueName[] = L"name";

const wchar_t kRegValueInstallerError[] = L"InstallerError";
const wchar_t kRegValueInstallerExtraCode1[] = L"InstallerExtraCode1";
const wchar_t kRegValueInstallerProgress[] = L"InstallerProgress";
const wchar_t kRegValueInstallerResult[] = L"InstallerResult";
const wchar_t kRegValueInstallerResultUIString[] = L"InstallerResultUIString";
const wchar_t kRegValueInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

const wchar_t kWindowsServiceName[] = L"UpdaterService";

}  // namespace updater
