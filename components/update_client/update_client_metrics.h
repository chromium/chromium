// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_

#include <string>

#include "base/time/time.h"
#include "components/update_client/crx_downloader.h"

namespace update_client::metrics {

void RecordCRXDownloadTime(base::TimeDelta time, const std::string& app_id);

void RecordCRXUnzipTime(base::TimeDelta time, const std::string& app_id);

void RecordCupValidationResult(bool valid);

void RecordCupValidationTime(base::TimeDelta time);

// Records the outcome of a single download attempt made by one of the
// downloaders in the CrxDownloader chain, keyed by the downloader which made
// the attempt: the result, the error if the attempt failed, and the download
// time if it succeeded.
void RecordCrxDownloadAttempt(const CrxDownloader::DownloadMetrics& metrics);

// Records which downloader in the CrxDownloader chain produced the file, or
// `kNone` if every downloader failed. Recorded once per download, after all
// attempts have completed.
void RecordCrxDownloadFinalDownloader(
    CrxDownloader::DownloadMetrics::Downloader downloader);

}  // namespace update_client::metrics

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_METRICS_H_
