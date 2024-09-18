// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_CONSTANTS_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_CONSTANTS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// Constants defining the IDs for the localized strings sent to the page as
// load time data.
extern const char kManagementScreenCaptureEvent[];
extern const char kManagementScreenCaptureData[];
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
extern const char kManagementDeviceSignalsDisclosure[];
#endif  // #if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kManagementLogUploadEnabled[];
extern const char kManagementLogUploadEnabledNoLink[];
extern const char kManagementReportActivityTimes[];
extern const char kManagementReportDeviceAudioStatus[];
extern const char kManagementReportDeviceGraphicsStatus[];
extern const char kManagementReportDevicePeripherals[];
extern const char kManagementReportNetworkData[];
extern const char kManagementReportHardwareData[];
extern const char kManagementReportUsers[];
extern const char kManagementReportCrashReports[];
extern const char kManagementReportAppInfoAndActivity[];
extern const char kManagementReportPrintJobs[];
extern const char kManagementReportDlpEvents[];
extern const char kManagementReportLoginLogout[];
extern const char kManagementReportCRDSessions[];
extern const char kManagementReportAllWebsiteInfoAndActivity[];
extern const char kManagementReportWebsiteInfoAndActivity[];
extern const char kManagementPrinting[];
extern const char kManagementCrostini[];
extern const char kManagementCrostiniContainerConfiguration[];
extern const char kManagementReportExtensions[];
extern const char kManagementReportAndroidApplications[];
extern const char kManagementOnFileTransferEvent[];
extern const char kManagementOnFileTransferVisibleData[];
extern const char kManagementReportFileEvents[];
#endif  // BUILDFLAG(IS_CHROMEOS)

extern const char kOnPremReportingExtensionStableId[];
extern const char kOnPremReportingExtensionBetaId[];

extern const char kManagementExtensionReportMachineName[];
extern const char kManagementExtensionReportMachineNameAddress[];
extern const char kManagementExtensionReportUsername[];
extern const char kManagementExtensionReportVersion[];
extern const char kManagementExtensionReportExtensionsPlugin[];
extern const char kManagementExtensionReportPerfCrash[];
extern const char kManagementExtensionReportUserBrowsingData[];
extern const char kManagementExtensionReportVisitedUrl[];

extern const char kThreatProtectionTitle[];
extern const char kManagementDataLossPreventionName[];
extern const char kManagementDataLossPreventionPermissions[];
extern const char kManagementMalwareScanningName[];
extern const char kManagementMalwareScanningPermissions[];
extern const char kManagementEnterpriseReportingEvent[];
extern const char kManagementEnterpriseReportingVisibleData[];
extern const char kManagementOnFileAttachedEvent[];
extern const char kManagementOnFileAttachedVisibleData[];
extern const char kManagementOnFileDownloadedEvent[];
extern const char kManagementOnFileDownloadedVisibleData[];
extern const char kManagementOnBulkDataEntryEvent[];
extern const char kManagementOnBulkDataEntryVisibleData[];
extern const char kManagementOnPrintEvent[];
extern const char kManagementOnPrintVisibleData[];
extern const char kManagementOnPageVisitedEvent[];
extern const char kManagementOnPageVisitedVisibleData[];
extern const char kManagementOnExtensionTelemetryEvent[];
extern const char kManagementOnExtensionTelemetryVisibleData[];

extern const char kManagementLegacyTechReport[];
extern const char kManagementLegacyTechReportNoLink[];

extern const char kPolicyKeyReportMachineIdData[];
extern const char kPolicyKeyReportUserIdData[];
extern const char kPolicyKeyReportVersionData[];
extern const char kPolicyKeyReportPolicyData[];
extern const char kPolicyKeyReportDlpEvents[];
extern const char kPolicyKeyReportExtensionsData[];
extern const char kPolicyKeyReportSystemTelemetryData[];
extern const char kPolicyKeyReportUserBrowsingData[];
extern const char kPolicyKeyReportVisitedUrlData[];

extern const char kReportingTypeDevice[];
extern const char kReportingTypeExtensions[];
extern const char kReportingTypeSecurity[];
extern const char kReportingTypeUser[];
extern const char kReportingTypeUserActivity[];
extern const char kReportingTypeLegacyTech[];
extern const char kReportingTypeUrl[];

extern const char kProfileReportingExplanation[];
extern const char kProfileReportingOverview[];
extern const char kProfileReportingUsername[];
extern const char kProfileReportingBrowser[];
extern const char kProfileReportingExtension[];
extern const char kProfileReportingPolicy[];
extern const char kProfileReportingLearnMore[];

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_CONSTANTS_H_
