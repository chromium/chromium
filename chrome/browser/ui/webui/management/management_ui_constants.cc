// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_constants.h"

const char kOnPremReportingExtensionStableId[] =
    "emahakmocgideepebncgnmlmliepgpgb";
const char kOnPremReportingExtensionBetaId[] =
    "kigjhoekjcpdfjpimbdjegmgecmlicaf";
const char kPolicyKeyReportMachineIdData[] = "report_machine_id_data";
const char kPolicyKeyReportUserIdData[] = "report_user_id_data";
const char kPolicyKeyReportVersionData[] = "report_version_data";
const char kPolicyKeyReportPolicyData[] = "report_policy_data";
const char kPolicyKeyReportExtensionsData[] = "report_extensions_data";
const char kPolicyKeyReportSystemTelemetryData[] =
    "report_system_telemetry_data";
const char kPolicyKeyReportUserBrowsingData[] = "report_user_browsing_data";
const char kPolicyKeyReportVisitedUrlData[] = "report_visited_url_data";

const char kManagementExtensionReportMachineName[] =
    "managementExtensionReportMachineName";
const char kManagementExtensionReportMachineNameAddress[] =
    "managementExtensionReportMachineNameAddress";
const char kManagementExtensionReportUsername[] =
    "managementExtensionReportUsername";
const char kManagementExtensionReportVersion[] =
    "managementExtensionReportVersion";
const char kManagementExtensionReportExtensionsPlugin[] =
    "managementExtensionReportExtensionsPlugin";
const char kManagementExtensionReportPerfCrash[] =
    "managementExtensionReportPerfCrash";
const char kManagementExtensionReportUserBrowsingData[] =
    "managementExtensionReportUserBrowsingData";
const char kManagementExtensionReportVisitedUrl[] =
    "managementExtensionReportVisitedUrl";

const char kThreatProtectionTitle[] = "threatProtectionTitle";
const char kManagementDataLossPreventionName[] =
    "managementDataLossPreventionName";
const char kManagementDataLossPreventionPermissions[] =
    "managementDataLossPreventionPermissions";
const char kManagementMalwareScanningName[] = "managementMalwareScanningName";
const char kManagementMalwareScanningPermissions[] =
    "managementMalwareScanningPermissions";
const char kManagementEnterpriseReportingEvent[] =
    "managementEnterpriseReportingEvent";
const char kManagementEnterpriseReportingVisibleData[] =
    "managementEnterpriseReportingVisibleData";

const char kManagementOnFileAttachedEvent[] = "managementOnFileAttachedEvent";
const char kManagementOnFileAttachedVisibleData[] =
    "managementOnFileAttachedVisibleData";
const char kManagementOnFileDownloadedEvent[] =
    "managementOnFileDownloadedEvent";
const char kManagementOnFileDownloadedVisibleData[] =
    "managementOnFileDownloadedVisibleData";
const char kManagementOnBulkDataEntryEvent[] = "managementOnBulkDataEntryEvent";
const char kManagementOnBulkDataEntryVisibleData[] =
    "managementOnBulkDataEntryVisibleData";
const char kManagementOnPrintEvent[] = "managementOnPrintEvent";
const char kManagementOnPrintVisibleData[] = "managementOnPrintVisibleData";

const char kManagementOnPageVisitedEvent[] = "managementOnPageVisitedEvent";
const char kManagementOnPageVisitedVisibleData[] =
    "managementOnPageVisitedVisibleData";
const char kManagementOnExtensionTelemetryEvent[] =
    "managementOnExtensionTelemetryEvent";
const char kManagementOnExtensionTelemetryVisibleData[] =
    "managementOnExtensionTelemetryVisibleData";

const char kManagementLegacyTechReport[] = "managementLegacyTechReport";
const char kManagementLegacyTechReportNoLink[] =
    "managementLegacyTechReportNoLink";

const char kReportingTypeDevice[] = "device";
const char kReportingTypeExtensions[] = "extensions";
const char kReportingTypeSecurity[] = "security";
const char kReportingTypeUser[] = "user";
const char kReportingTypeUserActivity[] = "user-activity";
const char kReportingTypeLegacyTech[] = "legacy-tech";
const char kReportingTypeUrl[] = "url";

const char kProfileReportingExplanation[] = "profileReportingExplanation";
const char kProfileReportingOverview[] = "profileReportingOverview";
const char kProfileReportingUsername[] = "profileReportingUsername";
const char kProfileReportingBrowser[] = "profileReportingBrowser";
const char kProfileReportingExtension[] = "profileReportingExtension";
const char kProfileReportingPolicy[] = "profileReportingPolicy";
const char kProfileReportingLearnMore[] = "profileReportingLearnMore";

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
const char kManagementScreenCaptureEvent[] = "managementScreenCaptureEvent";
const char kManagementScreenCaptureData[] = "managementScreenCaptureData";
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
const char kManagementDeviceSignalsDisclosure[] =
    "managementDeviceSignalsDisclosure";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
const char kManagementLogUploadEnabled[] = "managementLogUploadEnabled";
const char kManagementLogUploadEnabledNoLink[] =
    "managementLogUploadEnabledNoLink";
const char kManagementReportActivityTimes[] = "managementReportActivityTimes";
const char kManagementReportDeviceAudioStatus[] =
    "managementReportDeviceAudioStatus";
const char kManagementReportDeviceGraphicsStatus[] =
    "managementReportDeviceGraphicsStatus";
const char kManagementReportDevicePeripherals[] =
    "managementReportDevicePeripherals";
const char kManagementReportNetworkData[] = "managementReportNetworkData";
const char kManagementReportHardwareData[] = "managementReportHardwareData";
const char kManagementReportUsers[] = "managementReportUsers";
const char kManagementReportCrashReports[] = "managementReportCrashReports";
const char kManagementReportAppInfoAndActivity[] =
    "managementReportAppInfoAndActivity";
const char kManagementReportExtensions[] = "managementReportExtensions";
const char kManagementReportAndroidApplications[] =
    "managementReportAndroidApplications";
const char kManagementReportPrintJobs[] = "managementReportPrintJobs";
const char kManagementReportLoginLogout[] = "managementReportLoginLogout";
const char kManagementReportCRDSessions[] = "managementReportCRDSessions";
const char kManagementReportDlpEvents[] = "managementReportDlpEvents";
const char kManagementReportAllWebsiteInfoAndActivity[] =
    "managementReportAllWebsiteInfoAndActivity";
const char kManagementReportWebsiteInfoAndActivity[] =
    "managementReportWebsiteInfoAndActivity";
const char kManagementOnFileTransferEvent[] = "managementOnFileTransferEvent";
const char kManagementOnFileTransferVisibleData[] =
    "managementOnFileTransferVisibleData";
const char kManagementPrinting[] = "managementPrinting";
const char kManagementCrostini[] = "managementCrostini";
const char kManagementCrostiniContainerConfiguration[] =
    "managementCrostiniContainerConfiguration";
const char kManagementReportFileEvents[] = "managementReportFileEvents";
#endif  // BUILDFLAG(IS_CHROMEOS)
