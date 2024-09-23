// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_stats.h"

#include <map>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/safe_browsing/buildflags.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_util.h"

// TODO(crbug.com/40120259): Launch this on Fuchsia. We should also consider
// serving an empty FileTypePolicies to platforms without Safe Browsing to
// remove the BUILDFLAGs and nogncheck here.
#if (BUILDFLAG(FULL_SAFE_BROWSING) || BUILDFLAG(SAFE_BROWSING_DB_REMOTE)) && \
    !BUILDFLAG(IS_FUCHSIA)
#include "components/safe_browsing/content/common/file_type_policies.h"  // nogncheck
#endif

namespace download {
namespace {

// All possible error codes from the network module. Note that the error codes
// are all positive (since histograms expect positive sample values).
const int kAllInterruptReasonCodes[] = {
#define INTERRUPT_REASON(label, value) (value),
#include "components/download/public/common/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
};

// These values are based on net::HttpContentDisposition::ParseResult values.
// Values other than HEADER_PRESENT and IS_VALID are only measured if |IS_VALID|
// is true.
enum ContentDispositionCountTypes {
  // Count of downloads which had a Content-Disposition headers. The total
  // number of downloads is measured by UNTHROTTLED_COUNT.
  CONTENT_DISPOSITION_HEADER_PRESENT = 0,

  // Either 'filename' or 'filename*' attributes were valid and
  // yielded a non-empty filename.
  CONTENT_DISPOSITION_IS_VALID,

  // The following enum values correspond to
  // net::HttpContentDisposition::ParseResult.
  CONTENT_DISPOSITION_HAS_DISPOSITION_TYPE,
  CONTENT_DISPOSITION_HAS_UNKNOWN_TYPE,

  CONTENT_DISPOSITION_HAS_NAME,  // Obsolete; kept for UMA compatiblity.

  CONTENT_DISPOSITION_HAS_FILENAME,
  CONTENT_DISPOSITION_HAS_EXT_FILENAME,
  CONTENT_DISPOSITION_HAS_NON_ASCII_STRINGS,
  CONTENT_DISPOSITION_HAS_PERCENT_ENCODED_STRINGS,
  CONTENT_DISPOSITION_HAS_RFC2047_ENCODED_STRINGS,

  CONTENT_DISPOSITION_HAS_NAME_ONLY,  // Obsolete; kept for UMA compatiblity.

  CONTENT_DISPOSITION_HAS_SINGLE_QUOTED_FILENAME,

  CONTENT_DISPOSITION_LAST_ENTRY
};

// Helper method to calculate the bandwidth given the data length and time.
int64_t CalculateBandwidthBytesPerSecond(size_t length,
                                         base::TimeDelta elapsed_time) {
  int64_t elapsed_time_ms = elapsed_time.InMilliseconds();
  if (0 == elapsed_time_ms)
    elapsed_time_ms = 1;
  return 1000 * static_cast<int64_t>(length) / elapsed_time_ms;
}

// Records a histogram with download source suffix.
std::string CreateHistogramNameWithSuffix(const std::string& name,
                                          DownloadSource download_source) {
  std::string suffix;
  switch (download_source) {
    case DownloadSource::UNKNOWN:
      suffix = "UnknownSource";
      break;
    case DownloadSource::NAVIGATION:
      suffix = "Navigation";
      break;
    case DownloadSource::DRAG_AND_DROP:
      suffix = "DragAndDrop";
      break;
    case DownloadSource::FROM_RENDERER:
      suffix = "FromRenderer";
      break;
    case DownloadSource::EXTENSION_API:
      suffix = "ExtensionAPI";
      break;
    case DownloadSource::EXTENSION_INSTALLER:
      suffix = "ExtensionInstaller";
      break;
    case DownloadSource::INTERNAL_API:
      suffix = "InternalAPI";
      break;
    case DownloadSource::WEB_CONTENTS_API:
      suffix = "WebContentsAPI";
      break;
    case DownloadSource::OFFLINE_PAGE:
      suffix = "OfflinePage";
      break;
    case DownloadSource::CONTEXT_MENU:
      suffix = "ContextMenu";
      break;
    case DownloadSource::RETRY:
      suffix = "Retry";
      break;
    case DownloadSource::RETRY_FROM_BUBBLE:
      suffix = "RetryFromBubble";
      break;
    case DownloadSource::TOOLBAR_MENU:
      suffix = "ToolbarMenu";
      break;
  }

  return name + "." + suffix;
}

void RecordConnectionType(
    const std::string& name,
    net::NetworkChangeNotifier::ConnectionType connection_type,
    DownloadSource download_source) {
  using ConnectionType = net::NetworkChangeNotifier::ConnectionType;
  base::UmaHistogramExactLinear(name, connection_type,
                                ConnectionType::CONNECTION_LAST + 1);
  base::UmaHistogramExactLinear(
      CreateHistogramNameWithSuffix(name, download_source), connection_type,
      ConnectionType::CONNECTION_LAST + 1);
}

}  // namespace

void RecordDownloadCount(DownloadCountTypes type) {
  UMA_HISTOGRAM_ENUMERATION("Download.Counts", type,
                            DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordDownloadCountWithSource(DownloadCountTypes type,
                                   DownloadSource download_source) {
  RecordDownloadCount(type);

  std::string name =
      CreateHistogramNameWithSuffix("Download.Counts", download_source);
  base::UmaHistogramEnumeration(name, type, DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

void RecordNewDownloadStarted(
    net::NetworkChangeNotifier::ConnectionType connection_type,
    DownloadSource download_source) {
  RecordDownloadCountWithSource(NEW_DOWNLOAD_COUNT, download_source);
  RecordConnectionType("Download.NetworkConnectionType.StartNew",
                       connection_type, download_source);
}

void RecordDownloadCompleted(
    int64_t download_len,
    bool is_parallelizable,
    net::NetworkChangeNotifier::ConnectionType connection_type,
    DownloadSource download_source) {
  RecordDownloadCountWithSource(COMPLETED_COUNT, download_source);
  int64_t max = 1024 * 1024 * 1024;  // One Terabyte.
  download_len /= 1024;              // In Kilobytes
  UMA_HISTOGRAM_CUSTOM_COUNTS("Download.DownloadSize", download_len, 1, max,
                              256);
  if (is_parallelizable) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Download.DownloadSize.Parallelizable",
                                download_len, 1, max, 256);
  }
}

void RecordDownloadInterrupted(DownloadInterruptReason reason,
                               int64_t received,
                               int64_t total,
                               bool is_parallelizable,
                               bool is_parallel_download_enabled,
                               DownloadSource download_source) {
  RecordDownloadCountWithSource(INTERRUPTED_COUNT, download_source);
  if (is_parallelizable) {
    RecordParallelizableDownloadCount(INTERRUPTED_COUNT,
                                      is_parallel_download_enabled);
  }

  std::vector<base::HistogramBase::Sample> samples =
      base::CustomHistogram::ArrayToCustomEnumRanges(kAllInterruptReasonCodes);
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("Download.InterruptedReason", reason,
                                   samples);

  std::string name = CreateHistogramNameWithSuffix("Download.InterruptedReason",
                                                   download_source);
  base::HistogramBase* counter = base::CustomHistogram::FactoryGet(
      name, samples, base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(reason);

  if (is_parallel_download_enabled) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "Download.InterruptedReason.ParallelDownload", reason, samples);
  }

  int64_t delta_bytes = total - received;
  bool unknown_size = total <= 0;

  if (!unknown_size) {
    if (delta_bytes == 0) {
      RecordDownloadCountWithSource(INTERRUPTED_AT_END_COUNT, download_source);
      if (is_parallelizable) {
        RecordParallelizableDownloadCount(INTERRUPTED_AT_END_COUNT,
                                          is_parallel_download_enabled);
      }
    }
  }
}

void RecordDownloadRetry(DownloadInterruptReason reason) {
  std::vector<base::HistogramBase::Sample> samples =
      base::CustomHistogram::ArrayToCustomEnumRanges(kAllInterruptReasonCodes);
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("Download.Retry.InterruptReason", reason,
                                   samples);
}

void RecordDangerousDownloadAccept(DownloadDangerType danger_type,
                                   const base::FilePath& file_path) {
  UMA_HISTOGRAM_ENUMERATION("Download.UserValidatedDangerousDownload",
                            danger_type, DOWNLOAD_DANGER_TYPE_MAX);
}

namespace {

int GetMimeTypeMatch(const std::string& mime_type_string,
                     std::map<std::string, int> mime_type_map) {
  for (const auto& entry : mime_type_map) {
    if (entry.first == mime_type_string) {
      return entry.second;
    }
  }
  return 0;
}

static std::map<std::string, DownloadContent>
getMimeTypeToDownloadContentMap() {
  return {
      {"application/octet-stream", DownloadContent::OCTET_STREAM},
      {"binary/octet-stream", DownloadContent::OCTET_STREAM},
      {"application/pdf", DownloadContent::PDF},
      {"application/msword", DownloadContent::DOCUMENT},
      {"application/"
       "vnd.openxmlformats-officedocument.wordprocessingml.document",
       DownloadContent::DOCUMENT},
      {"application/rtf", DownloadContent::DOCUMENT},
      {"application/vnd.oasis.opendocument.text", DownloadContent::DOCUMENT},
      {"application/vnd.google-apps.document", DownloadContent::DOCUMENT},
      {"application/vnd.ms-excel", DownloadContent::SPREADSHEET},
      {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
       DownloadContent::SPREADSHEET},
      {"application/vnd.oasis.opendocument.spreadsheet",
       DownloadContent::SPREADSHEET},
      {"application/vnd.google-apps.spreadsheet", DownloadContent::SPREADSHEET},
      {"application/vns.ms-powerpoint", DownloadContent::PRESENTATION},
      {"application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation",
       DownloadContent::PRESENTATION},
      {"application/vnd.oasis.opendocument.presentation",
       DownloadContent::PRESENTATION},
      {"application/vnd.google-apps.presentation",
       DownloadContent::PRESENTATION},
      {"application/zip", DownloadContent::ARCHIVE},
      {"application/x-gzip", DownloadContent::ARCHIVE},
      {"application/x-rar-compressed", DownloadContent::ARCHIVE},
      {"application/x-tar", DownloadContent::ARCHIVE},
      {"application/x-bzip", DownloadContent::ARCHIVE},
      {"application/x-bzip2", DownloadContent::ARCHIVE},
      {"application/x-7z-compressed", DownloadContent::ARCHIVE},
      {"application/x-exe", DownloadContent::EXECUTABLE},
      {"application/java-archive", DownloadContent::EXECUTABLE},
      {"application/vnd.apple.installer+xml", DownloadContent::EXECUTABLE},
      {"application/x-csh", DownloadContent::EXECUTABLE},
      {"application/x-sh", DownloadContent::EXECUTABLE},
      {"application/x-apple-diskimage", DownloadContent::DMG},
      {"application/x-chrome-extension", DownloadContent::CRX},
      {"application/xhtml+xml", DownloadContent::WEB},
      {"application/xml", DownloadContent::WEB},
      {"application/javascript", DownloadContent::WEB},
      {"application/json", DownloadContent::WEB},
      {"application/typescript", DownloadContent::WEB},
      {"application/vnd.mozilla.xul+xml", DownloadContent::WEB},
      {"application/vnd.amazon.ebook", DownloadContent::EBOOK},
      {"application/epub+zip", DownloadContent::EBOOK},
      {"application/vnd.android.package-archive", DownloadContent::APK}};
}

// NOTE: Keep in sync with DownloadImageType in
// tools/metrics/histograms/enums.xml.
enum DownloadImage {
  DOWNLOAD_IMAGE_UNRECOGNIZED = 0,
  DOWNLOAD_IMAGE_GIF = 1,
  DOWNLOAD_IMAGE_JPEG = 2,
  DOWNLOAD_IMAGE_PNG = 3,
  DOWNLOAD_IMAGE_TIFF = 4,
  DOWNLOAD_IMAGE_ICON = 5,
  DOWNLOAD_IMAGE_WEBP = 6,
  DOWNLOAD_IMAGE_PSD = 7,
  DOWNLOAD_IMAGE_SVG = 8,
  DOWNLOAD_IMAGE_MAX = 9,
};

static std::map<std::string, int> getMimeTypeToDownloadImageMap() {
  return {{"image/gif", DOWNLOAD_IMAGE_GIF},
          {"image/jpeg", DOWNLOAD_IMAGE_JPEG},
          {"image/png", DOWNLOAD_IMAGE_PNG},
          {"image/tiff", DOWNLOAD_IMAGE_TIFF},
          {"image/vnd.microsoft.icon", DOWNLOAD_IMAGE_ICON},
          {"image/x-icon", DOWNLOAD_IMAGE_ICON},
          {"image/webp", DOWNLOAD_IMAGE_WEBP},
          {"image/vnd.adobe.photoshop", DOWNLOAD_IMAGE_PSD},
          {"image/svg+xml", DOWNLOAD_IMAGE_SVG}};
}

void RecordDownloadImageType(const std::string& mime_type_string) {
  DownloadImage download_image = DownloadImage(
      GetMimeTypeMatch(mime_type_string, getMimeTypeToDownloadImageMap()));
  UMA_HISTOGRAM_ENUMERATION("Download.ContentType.Image", download_image,
                            DOWNLOAD_IMAGE_MAX);
}

/** Text categories **/

// NOTE: Keep in sync with DownloadTextType in
// tools/metrics/histograms/enums.xml.
enum DownloadText {
  DOWNLOAD_TEXT_UNRECOGNIZED = 0,
  DOWNLOAD_TEXT_PLAIN = 1,
  DOWNLOAD_TEXT_CSS = 2,
  DOWNLOAD_TEXT_CSV = 3,
  DOWNLOAD_TEXT_HTML = 4,
  DOWNLOAD_TEXT_CALENDAR = 5,
  DOWNLOAD_TEXT_MAX = 6,
};

static std::map<std::string, int> getMimeTypeToDownloadTextMap() {
  return {{"text/plain", DOWNLOAD_TEXT_PLAIN},
          {"text/css", DOWNLOAD_TEXT_CSS},
          {"text/csv", DOWNLOAD_TEXT_CSV},
          {"text/html", DOWNLOAD_TEXT_HTML},
          {"text/calendar", DOWNLOAD_TEXT_CALENDAR}};
}

void RecordDownloadTextType(const std::string& mime_type_string) {
  DownloadText download_text = DownloadText(
      GetMimeTypeMatch(mime_type_string, getMimeTypeToDownloadTextMap()));
  UMA_HISTOGRAM_ENUMERATION("Download.ContentType.Text", download_text,
                            DOWNLOAD_TEXT_MAX);
}

/* Audio categories */

// NOTE: Keep in sync with DownloadAudioType in
// tools/metrics/histograms/enums.xml.
enum DownloadAudio {
  DOWNLOAD_AUDIO_UNRECOGNIZED = 0,
  DOWNLOAD_AUDIO_AAC = 1,
  DOWNLOAD_AUDIO_MIDI = 2,
  DOWNLOAD_AUDIO_OGA = 3,
  DOWNLOAD_AUDIO_WAV = 4,
  DOWNLOAD_AUDIO_WEBA = 5,
  DOWNLOAD_AUDIO_3GP = 6,
  DOWNLOAD_AUDIO_3G2 = 7,
  DOWNLOAD_AUDIO_MP3 = 8,
  DOWNLOAD_AUDIO_MAX = 9,
};

static std::map<std::string, int> getMimeTypeToDownloadAudioMap() {
  return {
      {"audio/aac", DOWNLOAD_AUDIO_AAC},   {"audio/midi", DOWNLOAD_AUDIO_MIDI},
      {"audio/ogg", DOWNLOAD_AUDIO_OGA},   {"audio/x-wav", DOWNLOAD_AUDIO_WAV},
      {"audio/webm", DOWNLOAD_AUDIO_WEBA}, {"audio/3gpp", DOWNLOAD_AUDIO_3GP},
      {"audio/3gpp2", DOWNLOAD_AUDIO_3G2}, {"audio/mp3", DOWNLOAD_AUDIO_MP3}};
}

void RecordDownloadAudioType(const std::string& mime_type_string) {
  DownloadAudio download_audio = DownloadAudio(
      GetMimeTypeMatch(mime_type_string, getMimeTypeToDownloadAudioMap()));
  UMA_HISTOGRAM_ENUMERATION("Download.ContentType.Audio", download_audio,
                            DOWNLOAD_AUDIO_MAX);
}

/* Video categories */

// NOTE: Keep in sync with DownloadVideoType in
// tools/metrics/histograms/enums.xml.
enum DownloadVideo {
  DOWNLOAD_VIDEO_UNRECOGNIZED = 0,
  DOWNLOAD_VIDEO_AVI = 1,
  DOWNLOAD_VIDEO_MPEG = 2,
  DOWNLOAD_VIDEO_OGV = 3,
  DOWNLOAD_VIDEO_WEBM = 4,
  DOWNLOAD_VIDEO_3GP = 5,
  DOWNLOAD_VIDEO_3G2 = 6,
  DOWNLOAD_VIDEO_MP4 = 7,
  DOWNLOAD_VIDEO_MOV = 8,
  DOWNLOAD_VIDEO_WMV = 9,
  DOWNLOAD_VIDEO_MAX = 10,
};

static std::map<std::string, int> getMimeTypeToDownloadVideoMap() {
  return {{"video/x-msvideo", DOWNLOAD_VIDEO_AVI},
          {"video/mpeg", DOWNLOAD_VIDEO_MPEG},
          {"video/ogg", DOWNLOAD_VIDEO_OGV},
          {"video/webm", DOWNLOAD_VIDEO_WEBM},
          {"video/3gpp", DOWNLOAD_VIDEO_3GP},
          {"video/3ggp2", DOWNLOAD_VIDEO_3G2},
          {"video/mp4", DOWNLOAD_VIDEO_MP4},
          {"video/quicktime", DOWNLOAD_VIDEO_MOV},
          {"video/x-ms-wmv", DOWNLOAD_VIDEO_WMV}};
}

void RecordDownloadVideoType(const std::string& mime_type_string) {
  DownloadVideo download_video = DownloadVideo(
      GetMimeTypeMatch(mime_type_string, getMimeTypeToDownloadVideoMap()));
  UMA_HISTOGRAM_ENUMERATION("Download.ContentType.Video", download_video,
                            DOWNLOAD_VIDEO_MAX);
}

// These histograms summarize download mime-types. The same data is recorded in
// a few places, as they exist to sanity-check and understand other metrics.
const char* const kDownloadMetricsVerificationNameItemSecure =
    "Download.InsecureBlocking.Verification.Item.Secure";
const char* const kDownloadMetricsVerificationNameItemInsecure =
    "Download.InsecureBlocking.Verification.Item.Insecure";
const char* const kDownloadMetricsVerificationNameItemOther =
    "Download.InsecureBlocking.Verification.Item.Other";
const char* const kDownloadMetricsVerificationNameManagerSecure =
    "Download.InsecureBlocking.Verification.Manager.Secure";
const char* const kDownloadMetricsVerificationNameManagerInsecure =
    "Download.InsecureBlocking.Verification.Manager.Insecure";
const char* const kDownloadMetricsVerificationNameManagerOther =
    "Download.InsecureBlocking.Verification.Manager.Other";

const char* GetDownloadValidationMetricName(
    const DownloadMetricsCallsite& callsite,
    const DownloadConnectionSecurity& state) {
  DCHECK(callsite == DownloadMetricsCallsite::kDownloadItem ||
         callsite == DownloadMetricsCallsite::kMixContentDownloadBlocking);

  switch (state) {
    case DOWNLOAD_SECURE:
    case DOWNLOAD_TARGET_BLOB:
    case DOWNLOAD_TARGET_DATA:
    case DOWNLOAD_TARGET_FILE:
      if (callsite == DownloadMetricsCallsite::kDownloadItem)
        return kDownloadMetricsVerificationNameItemSecure;
      return kDownloadMetricsVerificationNameManagerSecure;
    case DOWNLOAD_TARGET_INSECURE:
    case DOWNLOAD_REDIRECT_INSECURE:
    case DOWNLOAD_REDIRECT_TARGET_INSECURE:
      if (callsite == DownloadMetricsCallsite::kDownloadItem)
        return kDownloadMetricsVerificationNameItemInsecure;
      return kDownloadMetricsVerificationNameManagerInsecure;
    case DOWNLOAD_TARGET_OTHER:
    case DOWNLOAD_TARGET_FILESYSTEM:
    case DOWNLOAD_TARGET_FTP:
      if (callsite == DownloadMetricsCallsite::kDownloadItem)
        return kDownloadMetricsVerificationNameItemOther;
      return kDownloadMetricsVerificationNameManagerOther;
    case DOWNLOAD_CONNECTION_SECURITY_MAX:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace

DownloadContent DownloadContentFromMimeType(const std::string& mime_type_string,
                                            bool record_content_subcategory) {
  DownloadContent download_content = DownloadContent::UNRECOGNIZED;
  for (const auto& entry : getMimeTypeToDownloadContentMap()) {
    if (entry.first == mime_type_string) {
      download_content = entry.second;
    }
  }

  // Do partial matches.
  if (download_content == DownloadContent::UNRECOGNIZED) {
    if (base::StartsWith(mime_type_string, "text/",
                         base::CompareCase::SENSITIVE)) {
      download_content = DownloadContent::TEXT;
      if (record_content_subcategory)
        RecordDownloadTextType(mime_type_string);
    } else if (base::StartsWith(mime_type_string, "image/",
                                base::CompareCase::SENSITIVE)) {
      download_content = DownloadContent::IMAGE;
      if (record_content_subcategory)
        RecordDownloadImageType(mime_type_string);
    } else if (base::StartsWith(mime_type_string, "audio/",
                                base::CompareCase::SENSITIVE)) {
      download_content = DownloadContent::AUDIO;
      if (record_content_subcategory)
        RecordDownloadAudioType(mime_type_string);
    } else if (base::StartsWith(mime_type_string, "video/",
                                base::CompareCase::SENSITIVE)) {
      download_content = DownloadContent::VIDEO;
      if (record_content_subcategory)
        RecordDownloadVideoType(mime_type_string);
    } else if (base::StartsWith(mime_type_string, "font/",
                                base::CompareCase::SENSITIVE)) {
      download_content = DownloadContent::FONT;
    }
  }

  return download_content;
}

void RecordDownloadMimeType(const std::string& mime_type_string,
                            bool is_transient) {
  DownloadContent download_content =
      DownloadContentFromMimeType(mime_type_string, true);
  base::UmaHistogramEnumeration("Download.Start.ContentType", download_content,
                                DownloadContent::MAX);
#if BUILDFLAG(IS_ANDROID)
  base::UmaHistogramEnumeration(
      base::StrCat({"Download.Start.ContentType.",
                    is_transient ? "Transient" : "NonTransient"}),
      download_content, DownloadContent::MAX);
#endif  // BUILDFLAG(IS_ANDROID)
}

void RecordDownloadMimeTypeForNormalProfile(const std::string& mime_type_string,
                                            bool is_transient) {
  DownloadContent download_content =
      DownloadContentFromMimeType(mime_type_string, false);
  base::UmaHistogramEnumeration("Download.Start.ContentType.NormalProfile",
                                download_content, DownloadContent::MAX);
#if BUILDFLAG(IS_ANDROID)
  base::UmaHistogramEnumeration(
      base::StrCat({"Download.Start.ContentType.NormalProfile.",
                    is_transient ? "Transient" : "NonTransient"}),
      download_content, DownloadContent::MAX);
#endif  // BUILDFLAG(IS_ANDROID)
}

void RecordFileBandwidth(size_t length,
                         base::TimeDelta elapsed_time) {
  base::UmaHistogramCustomCounts(
      "Download.BandwidthOverallBytesPerSecond2",
      CalculateBandwidthBytesPerSecond(length, elapsed_time), 1,
      200 * 1000 * 1000, 50);
}

void RecordParallelizableDownloadCount(DownloadCountTypes type,
                                       bool is_parallel_download_enabled) {
  std::string histogram_name = is_parallel_download_enabled
                                   ? "Download.Counts.ParallelDownload"
                                   : "Download.Counts.ParallelizableDownload";
  base::UmaHistogramEnumeration(histogram_name, type,
                                DOWNLOAD_COUNT_TYPES_LAST_ENTRY);
}

namespace {
int g_parallel_download_creation_failure_count_ = 0;
}

void RecordParallelRequestCreationFailure(DownloadInterruptReason reason) {
  // This used to log a metric; however there is a test that checks how many
  // times that metric (and thus this method) was called. Ultimately that should
  // be refactored; but for now instead of logging the metric, just increment
  // a counter.
  g_parallel_download_creation_failure_count_++;
}

int GetParallelRequestCreationFailureCountForTesting() {
  return g_parallel_download_creation_failure_count_;
}

DownloadConnectionSecurity CheckDownloadConnectionSecurity(
    const GURL& download_url,
    const std::vector<GURL>& url_chain) {
  DownloadConnectionSecurity state = DOWNLOAD_TARGET_OTHER;
  if (download_url.SchemeIsHTTPOrHTTPS()) {
    bool is_final_download_secure = download_url.SchemeIsCryptographic();
    bool is_redirect_chain_secure = true;
    if (url_chain.size() > std::size_t(1)) {
      for (std::size_t i = std::size_t(0); i < url_chain.size() - 1; i++) {
        if (!url_chain[i].SchemeIsCryptographic()) {
          is_redirect_chain_secure = false;
          break;
        }
      }
    }
    state = is_final_download_secure
                ? is_redirect_chain_secure ? DOWNLOAD_SECURE
                                           : DOWNLOAD_REDIRECT_INSECURE
                : is_redirect_chain_secure ? DOWNLOAD_TARGET_INSECURE
                                           : DOWNLOAD_REDIRECT_TARGET_INSECURE;
  } else if (download_url.SchemeIsBlob()) {
    state = DOWNLOAD_TARGET_BLOB;
  } else if (download_url.SchemeIs(url::kDataScheme)) {
    state = DOWNLOAD_TARGET_DATA;
  } else if (download_url.SchemeIsFile()) {
    state = DOWNLOAD_TARGET_FILE;
  } else if (download_url.SchemeIsFileSystem()) {
    state = DOWNLOAD_TARGET_FILESYSTEM;
  } else if (download_url.SchemeIs(url::kFtpScheme)) {
    state = DOWNLOAD_TARGET_FTP;
  }
  return state;
}

void RecordDownloadValidationMetrics(DownloadMetricsCallsite callsite,
                                     DownloadConnectionSecurity state,
                                     DownloadContent file_type) {
  base::UmaHistogramEnumeration(
      GetDownloadValidationMetricName(callsite, state), file_type,
      DownloadContent::MAX);
}

void RecordDownloadHttpResponseCode(int response_code,
                                    bool is_background_mode) {
  int status_code = net::HttpUtil::MapStatusCodeForHistogram(response_code);
  std::vector<int> status_codes = net::HttpUtil::GetStatusCodesForHistogram();
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("Download.HttpResponseCode", status_code,
                                   status_codes);
  if (is_background_mode) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "Download.HttpResponseCode.BackgroundDownload", status_code,
        status_codes);
  }
}

void RecordInputStreamReadError(MojoResult mojo_result) {
  InputStreamReadError error = InputStreamReadError::kUnknown;
  switch (mojo_result) {
    case MOJO_RESULT_INVALID_ARGUMENT:
      error = InputStreamReadError::kInvalidArgument;
      break;
    case MOJO_RESULT_OUT_OF_RANGE:
      error = InputStreamReadError::kOutOfRange;
      break;
    case MOJO_RESULT_BUSY:
      error = InputStreamReadError::kBusy;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  base::UmaHistogramEnumeration("Download.InputStreamReadError", error);
}

#if BUILDFLAG(IS_ANDROID)
void RecordDuplicatePdfDownloadTriggered(bool open_inline) {
  base::UmaHistogramBoolean("Download.DuplicatePdfDownloadTriggered",
                            open_inline);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace download
