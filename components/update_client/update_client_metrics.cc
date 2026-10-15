// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/update_client/crx_downloader.h"

namespace update_client::metrics {

namespace {

// Returns the histogram variant name for `downloader`, or an empty view if
// `downloader` does not have a corresponding `CrxDownloaderType` token in
// //tools/metrics/histograms/metadata/others/histograms.xml.
std::string_view DownloaderToVariant(
    CrxDownloader::DownloadMetrics::Downloader downloader) {
  switch (downloader) {
    case CrxDownloader::DownloadMetrics::kNone:
      return "";
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
      return "UrlFetcher";
    case CrxDownloader::DownloadMetrics::kBits:
      return "Bits";
    case CrxDownloader::DownloadMetrics::kBackgroundMac:
      return "BackgroundMac";
    case CrxDownloader::DownloadMetrics::kDeliveryOptimization:
      return "DeliveryOptimization";
  }
  return "";
}

}  // namespace

void RecordCRXDownloadTime(base::TimeDelta time, const std::string& app_id) {
  base::UmaHistogramLongTimes(
      base::StrCat({"UpdateClient.DownloadTime2.", app_id}), time);
  base::UmaHistogramLongTimes("UpdateClient.DownloadTime2", time);
}

void RecordCRXUnzipTime(base::TimeDelta time, const std::string& app_id) {
  base::UmaHistogramMediumTimes(
      base::StrCat({"UpdateClient.UnzipTime.", app_id}), time);
  base::UmaHistogramMediumTimes("UpdateClient.UnzipTime", time);
}

void RecordCupValidationResult(bool valid) {
  base::UmaHistogramBoolean("UpdateClient.CupValidationResult", valid);
}

void RecordCupValidationTime(base::TimeDelta time) {
  base::UmaHistogramTimes("UpdateClient.CupValidationTime", time);
}

void RecordCrxDownloadAttempt(const CrxDownloader::DownloadMetrics& metrics) {
  const std::string_view variant = DownloaderToVariant(metrics.downloader);
  if (variant.empty()) {
    return;
  }
  const std::string prefix =
      base::StrCat({"UpdateClient.CrxDownloader.", variant});
  const bool success = metrics.error == 0;
  base::UmaHistogramBoolean(base::StrCat({prefix, ".Result"}), success);
  if (success) {
    base::UmaHistogramLongTimes(base::StrCat({prefix, ".DownloadTime"}),
                                base::Milliseconds(metrics.download_time_ms));
    return;
  }
  // The error codes of the Windows background downloaders are HRESULTs (along
  // with `CrxDownloaderError::BAD_HASH` when hash verification fails after a
  // successful transfer, and custom `FACILITY_ITF` HRESULTs such as
  // `BITS_TOO_MANY_JOBS` and `DO_TOO_MANY_DOWNLOADS`). The other downloaders
  // report a mix of net errors, HTTP status codes and CrxDownloaderError
  // values, which do not fit a single enum, so their error is not recorded
  // here.
  switch (metrics.downloader) {
    case CrxDownloader::DownloadMetrics::kBits:
    case CrxDownloader::DownloadMetrics::kDeliveryOptimization:
      base::UmaHistogramSparse(base::StrCat({prefix, ".Error"}), metrics.error);
      break;
    case CrxDownloader::DownloadMetrics::kNone:
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
    case CrxDownloader::DownloadMetrics::kBackgroundMac:
      break;
  }
}

void RecordCrxDownloadFinalDownloader(
    CrxDownloader::DownloadMetrics::Downloader downloader) {
  base::UmaHistogramEnumeration("UpdateClient.CrxDownloader.FinalDownloader",
                                downloader);
}

}  // namespace update_client::metrics
