// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_constants.h"

namespace google_update {

const wchar_t kChromeUpgradeCode[] = L"{8A69D345-D564-463C-AFF1-A69D9E530F96}";

const wchar_t kGoogleUpdateUpgradeCode[] =
    L"{430FD4D0-B729-4F61-AA34-91526481799D}";

const wchar_t kGoogleUpdateSetupExe[] = L"GoogleUpdateSetup.exe";

const wchar_t kRegPathClients[] = L"Software\\Google\\Update\\Clients";
const wchar_t kRegPathClientState[] = L"Software\\Google\\Update\\ClientState";
const wchar_t kRegPathClientStateMedium[] =
    L"Software\\Google\\Update\\ClientStateMedium";
const wchar_t kRegPathGoogleUpdate[] = L"Software\\Google\\Update";

const wchar_t kRegCommandsKey[] = L"Commands";

const wchar_t kRegAggregateMethod[] = L"aggregate";
const wchar_t kRegApField[] = L"ap";
const wchar_t kRegAutoRunOnOSUpgradeField[] = L"AutoRunOnOSUpgrade";
const wchar_t kRegBrandField[] = L"brand";
const wchar_t kRegBrowserField[] = L"browser";
const wchar_t kRegCFEndTempOptOutCmdField[] = L"CFEndTempOptOutCmd";
const wchar_t kRegCFOptInCmdField[] = L"CFOptInCmd";
const wchar_t kRegCFOptOutCmdField[] = L"CFOptOutCmd";
const wchar_t kRegCFTempOptOutCmdField[] = L"CFTempOptOutCmd";
const wchar_t kRegChannelField[] = L"channel";
const wchar_t kRegCleanInstallRequiredForVersionBelowField[] =
    L"CleanInstallRequiredForVersionBelow";
const wchar_t kRegClientField[] = L"client";
const wchar_t kRegCommandLineField[] = L"CommandLine";
const wchar_t kRegCriticalVersionField[] = L"cpv";
const wchar_t kRegDefaultField[] = L"";
const wchar_t kRegDidRunField[] = L"dr";
const wchar_t kRegDowngradeCleanupCommandField[] = L"DowngradeCleanupCommand";
const wchar_t kRegEulaAceptedField[] = L"eulaaccepted";
const wchar_t kRegGoogleUpdateVersion[] = L"version";
const wchar_t kRegInstallerProgress[] = L"InstallerProgress";
const wchar_t kRegLangField[] = L"lang";
const wchar_t kRegLastStartedAUField[] = L"LastStartedAU";
const wchar_t kRegLastCheckedField[] = L"LastChecked";
const wchar_t kRegLastCheckSuccessField[] = L"LastCheckSuccess";
const wchar_t kRegLastInstallerResultField[] = L"LastInstallerResult";
const wchar_t kRegLastInstallerErrorField[] = L"LastInstallerError";
const wchar_t kRegLastInstallerExtraField[] = L"LastInstallerExtraCode1";
const wchar_t kRegLastRunTimeField[] = L"lastrun";
const wchar_t kRegMetricsId[] = L"metricsid";
const wchar_t kRegMetricsIdEnabledDate[] = L"metricsid_enableddate";
const wchar_t kRegMetricsIdInstallDate[] = L"metricsid_installdate";
const wchar_t kRegMSIField[] = L"msi";
const wchar_t kRegNameField[] = L"name";
const wchar_t kRegOemInstallField[] = L"oeminstall";
const wchar_t kRegOldVersionField[] = L"opv";
const wchar_t kRegPathField[] = L"path";
const wchar_t kRegRLZBrandField[] = L"brand";
const wchar_t kRegRLZReactivationBrandField[] = L"reactivationbrand";
const wchar_t kRegReferralField[] = L"referral";
const wchar_t kRegRunAsUserField[] = L"RunAsUser";
const wchar_t kRegSendsPingsField[] = L"SendsPings";
const wchar_t kRegUninstallCmdLine[] = L"UninstallCmdLine";
const wchar_t kRegUsageStatsField[] = L"usagestats";
const wchar_t kRegVersionField[] = L"pv";
const wchar_t kRegWebAccessibleField[] = L"WebAccessible";

}  // namespace google_update
