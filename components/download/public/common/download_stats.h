// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Holds helpers for gathering UMA stats about downloads.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_STATS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/download/public/common/download_content.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_source.h"
#include "mojo/public/c/system/types.h"
#include "net/base/network_change_notifier.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class Time;
class TimeDelta;
}  // namespace base

namespace download {

// We keep a count of how often various events occur in the
// histogram "Download.Counts".
enum DownloadCountTypes {
  // Stale enum values left around so that values passed to UMA don't
  // change.
  DOWNLOAD_COUNT_UNUSED_0 = 0,
  DOWNLOAD_COUNT_UNUSED_1,
  DOWNLOAD_COUNT_UNUSED_2,
  DOWNLOAD_COUNT_UNUSED_3,
  DOWNLOAD_COUNT_UNUSED_4,

  // Downloads that made it to DownloadResourceHandler
  UNTHROTTLED_COUNT,

  // Downloads that actually complete.
  COMPLETED_COUNT,

  // Downloads that are cancelled before completion (user action or error).
  CANCELLED_COUNT,

  // Downloads that are started.
  START_COUNT,

  // Downloads that were interrupted by the OS.
  INTERRUPTED_COUNT,

  // (Deprecated) Write sizes for downloads.
  // This is equal to the number of samples in Download.WriteSize histogram.
  DOWNLOAD_COUNT_UNUSED_10,

  // (Deprecated) Counts iterations of the BaseFile::AppendDataToFile() loop.
  // This is equal to the number of samples in Download.WriteLoopCount
  // histogram.
  DOWNLOAD_COUNT_UNUSED_11,

  // Counts interruptions that happened at the end of the download.
  INTERRUPTED_AT_END_COUNT,

  // Counts errors due to writes to BaseFiles that have been detached already.
  // This can happen when saving web pages as complete packages. It happens
  // when we get messages to append data to files that have already finished and
  // been detached, but haven't yet been removed from the list of files in
  // progress.
  APPEND_TO_DETACHED_FILE_COUNT,

  // (Deprecated) Counts the number of instances where the downloaded file is
  // missing after a successful invocation of ScanAndSaveDownloadedFile().
  DOWNLOAD_COUNT_UNUSED_14,

  // (Deprecated) Count of downloads with a strong ETag and specified
  // 'Accept-Ranges: bytes'.
  DOWNLOAD_COUNT_UNUSED_15,

  // Count of downloads that didn't have a valid WebContents at the time it was
  // interrupted.
  INTERRUPTED_WITHOUT_WEBCONTENTS,

  // Count of downloads that supplies a strong validator (implying byte-wise
  // equivalence) and has a 'Accept-Ranges: bytes' header. These downloads are
  // candidates for partial resumption.
  STRONG_VALIDATOR_AND_ACCEPTS_RANGES,

  // (Deprecated) Count of downloads that uses parallel download requests.
  USES_PARALLEL_REQUESTS,

  // Count of new downloads.
  NEW_DOWNLOAD_COUNT,

  // Count of new downloads that are started in normal profile.
  NEW_DOWNLOAD_COUNT_NORMAL_PROFILE,

  // Downloads that are actually completed in normal profile.
  COMPLETED_COUNT_NORMAL_PROFILE,

  // Downloads that are completed with a content length mismatch error.
  COMPLETED_WITH_CONTENT_LENGTH_MISMATCH_COUNT,

  // After a download is interrupted with a content length mismatch error, more
  // bytes are received when resuming the download.
  MORE_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT,

  // After a download is interrupted with a content length mismatch error, no
  // bytes are received when resuming the download.
  NO_BYTES_RECEIVED_AFTER_CONTENT_LENGTH_MISMATCH_COUNT,

  // Count of downloads that requested target determination.
  DETERMINE_DOWNLOAD_TARGET_COUNT,

  // Count of downloads that has target determination completed.
  DOWNLOAD_TARGET_DETERMINED_COUNT,

  // A cross origin download without a content disposition header.
  CROSS_ORIGIN_DOWNLOAD_WITHOUT_CONTENT_DISPOSITION,

  // Count of attempts that triggered the download flow, before any network
  // requests are sent.
  DOWNLOAD_TRIGGERED_COUNT,

  // Count of attempts for manual download resumption.
  MANUAL_RESUMPTION_COUNT,

  // Count of attempts for auto download resumption.
  AUTO_RESUMPTION_COUNT,

  // Count of download attempts that are dropped due to content settings or
  // request limiter before DownloadItem is created.
  DOWNLOAD_DROPPED_COUNT,

  DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

// Events for user scheduled downloads. Used in histograms, don't reuse or
// remove items. Keep in sync with DownloadLaterEvent in enums.xml.
enum class DownloadLaterEvent {
  // Schedule is added during download target determination process.
  kScheduleAdded = 0,
  // Scheduled is changed from the UI after download is scheduled.
  kScheduleChanged = 1,
  // Scheduled is removed during resumption.
  kScheduleRemoved = 2,
  kMaxValue = kScheduleRemoved
};

// Increment one of the above counts.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadCount(DownloadCountTypes type);

// Record download count with download source.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadCountWithSource(
    DownloadCountTypes type,
    DownloadSource download_source);

// Record metrics when a new download is started.
COMPONENTS_DOWNLOAD_EXPORT void RecordNewDownloadStarted(
    net::NetworkChangeNotifier::ConnectionType connection_type,
    DownloadSource download_source);

// Record COMPLETED_COUNT and how long the download took.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadCompleted(
    int64_t download_len,
    bool is_parallelizable,
    net::NetworkChangeNotifier::ConnectionType connection_type,
    DownloadSource download_source);

// Record INTERRUPTED_COUNT, |reason|, |received| and |total| bytes.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadInterrupted(
    DownloadInterruptReason reason,
    int64_t received,
    int64_t total,
    bool is_parallelizable,
    bool is_parallel_download_enabled,
    DownloadSource download_source);

// Record a dangerous download accept event.
COMPONENTS_DOWNLOAD_EXPORT void RecordDangerousDownloadAccept(
    DownloadDangerType danger_type,
    const base::FilePath& file_path);

// Records the interrupt reason when a download is retried.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadRetry(
    DownloadInterruptReason reason);

// Returns the type of download.
COMPONENTS_DOWNLOAD_EXPORT DownloadContent
DownloadContentFromMimeType(const std::string& mime_type_string,
                            bool record_content_subcategory);

// Records the mime type of the download.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadMimeType(
    const std::string& mime_type,
    bool is_transient);

// Records the mime type of the download for normal profile.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadMimeTypeForNormalProfile(
    const std::string& mime_type,
    bool is_transient);

// Record overall bandwidth stats at the file end.
// Does not count in any hash computation or file open/close time.
COMPONENTS_DOWNLOAD_EXPORT void RecordFileBandwidth(
    size_t length,
    base::TimeDelta elapsed_time);

// Increment one of the count for parallelizable download.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelizableDownloadCount(
    DownloadCountTypes type,
    bool is_parallel_download_enabled);

// Record the result of a download file rename.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadFileRenameResultAfterRetry(
    base::TimeDelta time_since_first_failure,
    DownloadInterruptReason interrupt_reason);

// Enumeration for histogramming purposes.
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum DownloadConnectionSecurity {
  DOWNLOAD_SECURE = 0,  // Final download url and its redirects all use https
  DOWNLOAD_TARGET_INSECURE =
      1,  // Final download url uses http, redirects are all
          // https
  DOWNLOAD_REDIRECT_INSECURE =
      2,  // Final download url uses https, but at least
          // one redirect uses http
  DOWNLOAD_REDIRECT_TARGET_INSECURE =
      3,                      // Final download url uses http, and at
                              // least one redirect uses http
  DOWNLOAD_TARGET_OTHER = 4,  // Final download url uses a scheme not present in
                              // this enumeration
  DOWNLOAD_TARGET_BLOB = 5,   // Final download url uses blob scheme
  DOWNLOAD_TARGET_DATA = 6,   //  Final download url uses data scheme
  DOWNLOAD_TARGET_FILE = 7,   //  Final download url uses file scheme
  DOWNLOAD_TARGET_FILESYSTEM = 8,  //  Final download url uses filesystem scheme
  DOWNLOAD_TARGET_FTP = 9,         // Final download url uses ftp scheme
  DOWNLOAD_CONNECTION_SECURITY_MAX
};

enum class DownloadMetricsCallsite {
  // Called from within DownloadItem initialization.
  kDownloadItem = 0,

  // Called from within MixedContentDownloadBlocking (as part of
  // ChromeDownloadManagerDelegate).
  kMixContentDownloadBlocking,
};

enum class InputStreamReadError {
  // Reading the input stream cause a mojo input argument error.
  kInvalidArgument = 0,

  // Reading the input stream cause a mojo out of range error.
  kOutOfRange = 1,

  // Reading the input stream cause a mojo busy error.
  kBusy = 2,

  kUnknown = 3,
  kMaxValue = kUnknown,
};

COMPONENTS_DOWNLOAD_EXPORT DownloadConnectionSecurity
CheckDownloadConnectionSecurity(const GURL& download_url,
                                const std::vector<GURL>& url_chain);

// Records a download's mime-type and security state. This is a short-lived
// metric recorded in multiple callsites to investigate discrepancies in other
// metrics.
COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadValidationMetrics(
    DownloadMetricsCallsite callsite,
    DownloadConnectionSecurity state,
    DownloadContent file_type);

COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadConnectionSecurity(
    const GURL& download_url,
    const std::vector<GURL>& url_chain);

COMPONENTS_DOWNLOAD_EXPORT void RecordDownloadHttpResponseCode(
    int response_code,
    bool is_background_mode);

// Records the interrupt reason that causes download to restart.
COMPONENTS_DOWNLOAD_EXPORT void RecordResumptionStrongValidators(
    DownloadInterruptReason reason);

// TODO(crbug.com/40283525): This is only used for the purposes of tests
// and should be refactored.
COMPONENTS_DOWNLOAD_EXPORT void RecordParallelRequestCreationFailure(
    DownloadInterruptReason reason);

COMPONENTS_DOWNLOAD_EXPORT int
GetParallelRequestCreationFailureCountForTesting();

// Records the input stream read error type.
COMPONENTS_DOWNLOAD_EXPORT void RecordInputStreamReadError(
    MojoResult mojo_result);

#if BUILDFLAG(IS_ANDROID)
enum class BackgroudTargetDeterminationResultTypes {
  // Target determination succeeded.
  kSuccess = 0,

  // Target path doesn't exist.
  kTargetPathMissing = 1,

  // Path reservation failed.
  kPathReservationFailed = 2,

  kMaxValue = kPathReservationFailed
};

COMPONENTS_DOWNLOAD_EXPORT void RecordDuplicatePdfDownloadTriggered(
    bool open_inline);

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_STATS_H_
