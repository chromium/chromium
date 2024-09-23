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

const wchar_t kRegKeyCommands[] = L"Commands";
const wchar_t kRegValueCommandLine[] = L"CommandLine";
const wchar_t kRegValueAutoRunOnOSUpgrade[] = L"AutoRunOnOSUpgrade";

const wchar_t kRegValuePV[] = L"pv";
const wchar_t kRegValueBrandCode[] = L"brand";
const wchar_t kRegValueAP[] = L"ap";
const wchar_t kRegValueDateOfLastActivity[] = L"DayOfLastActivity";
const wchar_t kRegValueDateOfLastRollcall[] = L"DayOfLastRollCall";
const wchar_t kRegValueDayOfInstall[] = L"DayOfInstall";
const wchar_t kRegValueName[] = L"name";
const wchar_t kRegValueUninstallCmdLine[] = L"UninstallCmdLine";
const wchar_t kRegValueVersion[] = L"version";

const wchar_t kRegValueOemInstallTimeMin[] = L"OemInstallTime";
const wchar_t kSetupStateKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Setup\\State";
const wchar_t kImageStateValueName[] = L"ImageState";
const wchar_t kImageStateUnuseableValue[] = L"IMAGE_STATE_UNDEPLOYABLE";
const wchar_t kImageStateGeneralAuditValue[] =
    L"IMAGE_STATE_GENERALIZE_RESEAL_TO_AUDIT";
const wchar_t kImageStateSpecialAuditValue[] =
    L"IMAGE_STATE_SPECIALIZE_RESEAL_TO_AUDIT";

const wchar_t kRegKeyCohort[] = L"cohort";
const wchar_t kRegValueCohortName[] = L"name";
const wchar_t kRegValueCohortHint[] = L"hint";

const wchar_t kRegValueInstallerError[] = L"InstallerError";
const wchar_t kRegValueInstallerExtraCode1[] = L"InstallerExtraCode1";
const wchar_t kRegValueInstallerProgress[] = L"InstallerProgress";
const wchar_t kRegValueInstallerResult[] = L"InstallerResult";
const wchar_t kRegValueInstallerResultUIString[] = L"InstallerResultUIString";
const wchar_t kRegValueInstallerSuccessLaunchCmdLine[] =
    L"InstallerSuccessLaunchCmdLine";

const wchar_t kRegValueLastInstallerResult[] = L"LastInstallerResult";
const wchar_t kRegValueLastInstallerError[] = L"LastInstallerError";
const wchar_t kRegValueLastInstallerExtraCode1[] = L"LastInstallerExtraCode1";
const wchar_t kRegValueLastInstallerResultUIString[] =
    L"LastInstallerResultUIString";
const wchar_t kRegValueLastInstallerSuccessLaunchCmdLine[] =
    L"LastInstallerSuccessLaunchCmdLine";

const wchar_t* const kRegValuesLastInstaller[5] = {
    kRegValueLastInstallerResult, kRegValueLastInstallerError,
    kRegValueLastInstallerExtraCode1, kRegValueLastInstallerResultUIString,
    kRegValueLastInstallerSuccessLaunchCmdLine};

const wchar_t kRegKeyCompanyCloudManagement[] =
    COMPANY_POLICIES_KEY L"CloudManagement\\";
const wchar_t kRegValueEnrollmentToken[] = L"EnrollmentToken";

const wchar_t kRegKeyCompanyLegacyCloudManagement[] =
    COMPANY_POLICIES_KEY BROWSER_NAME_STRING L"\\";
const wchar_t kRegValueCloudManagementEnrollmentToken[] =
    L"CloudManagementEnrollmentToken";

const wchar_t kRegValueEnrollmentMandatory[] = L"EnrollmentMandatory";

const wchar_t kRegKeyCompanyEnrollment[] = COMPANY_KEY L"Enrollment\\";
const wchar_t kRegKeyCompanyLegacyEnrollment[] =
    COMPANY_KEY L"\\" BROWSER_NAME_STRING L"\\Enrollment\\";
const wchar_t kRegValueDmToken[] = L"dmtoken";

const wchar_t kWindowsServiceName[] = L"Service";
const wchar_t kWindowsInternalServiceName[] = L"InternalService";

const wchar_t kShutdownEvent[] = L"{A0C1F415-D2CE-4ddc-9B48-14E56FD55162}";

const wchar_t kLegacyExeName[] = COMPANY_SHORTNAME_STRING L"Update.exe";

const wchar_t kLegacyServiceNamePrefix[] = L"" LEGACY_SERVICE_NAME_PREFIX;

const wchar_t kLegacyServiceDisplayNamePrefix[] =
    COMPANY_SHORTNAME_STRING L" Update Service";

const wchar_t kLegacyRunValuePrefix[] = COMPANY_SHORTNAME_STRING L" Update";

const wchar_t kLegacyTaskNamePrefixSystem[] =
    COMPANY_SHORTNAME_STRING L"UpdateTaskMachine";
const wchar_t kLegacyTaskNamePrefixUser[] =
    COMPANY_SHORTNAME_STRING L"UpdateTaskUser";

}  // namespace updater
