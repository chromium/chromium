// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/win_constants.h"

namespace updater {

const wchar_t kLegacyGoogleUpdateAppID[] = L"" LEGACY_GOOGLE_UPDATE_APPID;

const wchar_t kGoogleUpdate3WebSystemClassProgId[] =
    COMPANY_SHORTNAME_STRING L"Update.Update3WebMachine";
const wchar_t kGoogleUpdate3WebUserClassProgId[] =
    COMPANY_SHORTNAME_STRING L"Update.Update3WebUser";

const wchar_t kGlobalPrefix[] = L"Global\\G";

const wchar_t kPrefsAccessMutex[] = L"" PREFS_ACCESS_MUTEX;

const wchar_t kRegKeyCommands[] = L"Commands";
const wchar_t kRegValueCommandLine[] = L"CommandLine";
const wchar_t kRegValueAutoRunOnOSUpgrade[] = L"AutoRunOnOSUpgrade";

const wchar_t kRegValuePV[] = L"pv";
const wchar_t kRegValueBrandCode[] = L"brand";
const wchar_t kRegValueAP[] = L"ap";
const wchar_t kRegValueName[] = L"name";
const wchar_t kRegValueUninstallCmdLine[] = L"UninstallCmdLine";

const wchar_t kRegValueInstallerError[] = L"InstallerError";
const wchar_t kRegValueInstallerExtraCode1[] = L"InstallerExtraCode1";
const wchar_t kRegValueInstallerProgress[] = L"InstallerProgress";
const wchar_t kRegValueInstallerResult[] = L"InstallerResult";
const wchar_t kRegValueInstallerResultUIString[] = L"InstallerResultUIString";
const wchar_t kRegValueInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

const wchar_t kRegKeyCompanyCloudManagement[] =
    COMPANY_POLICIES_KEY L"CloudManagement\\";
const wchar_t kRegValueEnrollmentToken[] = L"EnrollmentToken\\";

const wchar_t kRegValueEnrollmentMandatory[] = L"EnrollmentMandatory";

const wchar_t kRegKeyCompanyEnrollment[] = COMPANY_KEY L"Enrollment\\";
const wchar_t kRegValueDmToken[] = L"dmtoken";

const wchar_t kWindowsServiceName[] = L"Service";
const wchar_t kWindowsInternalServiceName[] = L"InternalService";

const wchar_t kShutdownEvent[] = L"{A0C1F415-D2CE-4ddc-9B48-14E56FD55162}";

const wchar_t kLegacyExeName[] = COMPANY_SHORTNAME_STRING L"Update.exe";

}  // namespace updater
