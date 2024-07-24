// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DLP_HISTOGRAM_HELPER_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DLP_HISTOGRAM_HELPER_H_

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/enterprise/data_controls/core/browser/rule.h"

namespace data_controls {

namespace dlp {

// Constants with UMA histogram name suffixes.
inline constexpr char kCaptureModeInitBlockedUMA[] = "CaptureModeInitBlocked";
inline constexpr char kCaptureModeInitWarnedUMA[] = "CaptureModeInitWarned";
inline constexpr char kClipboardReadBlockedUMA[] = "ClipboardReadBlocked";
inline constexpr char kDataTransferReportingTimeDiffUMA[] =
    "DataTransferReportingTimeDiff";
inline constexpr char kDataTransferControllerStartedUMA[] =
    "DataTransferControllerStarted";
inline constexpr char kDlpPolicyPresentUMA[] = "DlpPolicyPresent";
inline constexpr char kDragDropBlockedUMA[] = "DragDropBlocked";
inline constexpr char kFileActionBlockedUMA[] = "FileActionBlocked2";
inline constexpr char kFileActionBlockReviewedUMA[] = "FileActionBlockReviewed";
inline constexpr char kFilesBlockedCountUMA[] = "FilesBlockedCount";
inline constexpr char kFileActionWarnedUMA[] = "FileActionWarned";
inline constexpr char kFileActionWarnProceededUMA[] = "FileActionWarnProceeded";
inline constexpr char kFileActionWarnTimedOutUMA[] = "FileActionWarnTimedOut";
inline constexpr char kFileActionWarnReviewedUMA[] = "FileActionWarnReviewed";
inline constexpr char kFilesWarnedCountUMA[] = "FilesWarnedCount";
inline constexpr char kFilesAppOpenTimedOutUMA[] = "FilesAppOpenTimedOut";
inline constexpr char kFilesDaemonStartedUMA[] = "FilesDaemonStarted";
inline constexpr char kSameFileEventTimeDiffUMA[] = "SameFileEventTimeDiff";
inline constexpr char kPrintingBlockedUMA[] = "PrintingBlocked";
inline constexpr char kPrintingWarnedUMA[] = "PrintingWarned";
inline constexpr char kPrintingWarnProceededUMA[] = "PrintingWarnProceeded";
inline constexpr char kPrintingWarnSilentProceededUMA[] =
    "PrintingWarnSilentProceeded";
inline constexpr char kPrivacyScreenEnforcedUMA[] = "PrivacyScreenEnforced";
inline constexpr char kScreenShareBlockedUMA[] = "ScreenShareBlocked";
inline constexpr char kScreenShareWarnedUMA[] = "ScreenShareWarned";
inline constexpr char kScreenShareWarnProceededUMA[] =
    "ScreenShareWarnProceeded";
inline constexpr char kScreenShareWarnSilentProceededUMA[] =
    "ScreenShareWarnSilentProceeded";
inline constexpr char kScreenSharePausedOrResumedUMA[] =
    "ScreenSharePausedOrResumed";
inline constexpr char kScreenshotBlockedUMA[] = "ScreenshotBlocked";
inline constexpr char kScreenshotWarnedUMA[] = "ScreenshotWarned";
inline constexpr char kScreenshotWarnProceededUMA[] = "ScreenshotWarnProceeded";
inline constexpr char kScreenshotWarnSilentProceededUMA[] =
    "ScreenshotWarnSilentProceeded";
inline constexpr char kVideoCaptureInterruptedUMA[] = "VideoCaptureInterrupted";
inline constexpr char kReportedBlockLevelRestriction[] =
    "ReportedBlockLevelRestriction";
inline constexpr char kReportedReportLevelRestriction[] =
    "ReportedReportLevelRestriction";
inline constexpr char kReportedWarnLevelRestriction[] =
    "ReportedWarnLevelRestriction";
inline constexpr char kReportedWarnProceedLevelRestriction[] =
    "ReportedWarnProceedLevelRestriction";
inline constexpr char kReportedEventStatus[] = "ReportedEventStatus";
inline constexpr char kConfidentialContentsCount[] =
    "ConfidentialContentsCount";
inline constexpr char kActiveFileEventsCount[] = "ActiveFileEventsCount";
inline constexpr char kErrorsReportQueueNotReady[] =
    "Errors.ReportQueueNotReady";
inline constexpr char kErrorsFilesPolicySetup[] = "Errors.FilesPolicySetup";
inline constexpr char kFilesUnknownAccessLevel[] = "FilesUnknownAccessLevel";
inline constexpr char kFilesDefaultFileAccess[] = "FilesDefaultFileAccess";

}  // namespace dlp

std::string GetDlpHistogramPrefix();

void DlpBooleanHistogram(const std::string& suffix, bool value);

void DlpCountHistogram(const std::string& suffix, int sample, int max);

void DlpRestrictionConfiguredHistogram(Rule::Restriction value);

template <typename T>
void DlpHistogramEnumeration(const std::string& suffix, T sample) {
  base::UmaHistogramEnumeration(GetDlpHistogramPrefix() + suffix, sample);
}

void DlpCountHistogram10000(const std::string& suffix, int sample);

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DLP_HISTOGRAM_HELPER_H_
