// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_download.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/update_client/cancellation.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_engine.h"
#include "url/gurl.h"

namespace update_client {

namespace {

#if BUILDFLAG(IS_MAC)
// The minimum size of a download to attempt it at background priority.
constexpr int64_t kBackgroundDownloadSizeThreshold = 10000000; /*10 MB*/
#endif

bool CanDoBackgroundDownload(scoped_refptr<const UpdateContext> update_context,
                             int64_t size) {
  // Foreground component updates are always downloaded in foreground.
  bool enabled = !update_context->is_foreground &&
                 update_context->config->EnabledBackgroundDownloader();
#if BUILDFLAG(IS_MAC)
  enabled &= size > kBackgroundDownloadSizeThreshold;
#endif
  return enabled;
}

// Returns a string literal corresponding to the value of the downloader |d|.
const char* DownloaderToString(CrxDownloader::DownloadMetrics::Downloader d) {
  switch (d) {
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
      return "direct";
    case CrxDownloader::DownloadMetrics::kBits:
      return "bits";
    case CrxDownloader::DownloadMetrics::kBackgroundMac:
      return "nsurlsession_background";
    default:
      return "unknown";
  }
}

base::Value::Dict MakeEvent(const CrxDownloader::DownloadMetrics& dm) {
  base::Value::Dict event;
  event.Set("eventtype", protocol_request::kEventDownload);
  event.Set("eventresult", static_cast<int>(dm.error == 0));
  event.Set("downloader", DownloaderToString(dm.downloader));
  if (dm.error) {
    event.Set("errorcode", dm.error);
  }
  if (dm.extra_code1) {
    event.Set("extracode1", dm.extra_code1);
  }
  event.Set("url", dm.url.spec());

  // -1 means that the  byte counts are not known.
  if (dm.total_bytes >= 0 &&
      dm.total_bytes < protocol_request::kProtocolMaxInt) {
    event.Set("total", static_cast<double>(dm.total_bytes));
  }
  if (dm.downloaded_bytes >= 0 &&
      dm.downloaded_bytes < protocol_request::kProtocolMaxInt) {
    event.Set("downloaded", static_cast<double>(dm.downloaded_bytes));
  }
  if (dm.download_time_ms >= 0 &&
      dm.download_time_ms < protocol_request::kProtocolMaxInt) {
    event.Set("download_time_ms", static_cast<double>(dm.download_time_ms));
  }
  return event;
}

void DownloadComplete(
    scoped_refptr<CrxDownloader> crx_downloader,
    scoped_refptr<Cancellation> cancellation,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    base::OnceCallback<
        void(const base::expected<base::FilePath, CategorizedError>&)> callback,
    const CrxDownloader::Result& download_result) {
  cancellation->Clear();

  for (const auto& metric : crx_downloader->download_metrics()) {
    event_adder.Run(MakeEvent(metric));
  }

  if (cancellation->IsCancelled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected<CategorizedError>(
                {.category_ = ErrorCategory::kService,
                 .code_ = static_cast<int>(ServiceError::CANCELLED)})));
    return;
  }

  if (download_result.error) {
    CHECK(download_result.response.empty());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected<CategorizedError>(
                           {.category_ = ErrorCategory::kDownload,
                            .code_ = download_result.error,
                            .extra_ = download_result.extra_code1})));
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), download_result.response));
}

void HandleAvailableSpace(
    scoped_refptr<const UpdateContext> update_context,
    scoped_refptr<Cancellation> cancellation,
    const std::vector<GURL>& urls,
    int64_t size,
    const std::string& hash,
    CrxDownloader::ProgressCallback progress_callback,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    base::OnceCallback<
        void(const base::expected<base::FilePath, CategorizedError>&)> callback,
    int64_t available_bytes) {
  if (available_bytes / 2 <= size) {
    VLOG(1) << "available_bytes: " << available_bytes
            << ", download size: " << size;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected<CategorizedError>(
                {.category_ = ErrorCategory::kDownload,
                 .code_ = static_cast<int>(CrxDownloaderError::DISK_FULL)})));
    return;
  }
  scoped_refptr<CrxDownloader> crx_downloader =
      update_context->config->GetCrxDownloaderFactory()->MakeCrxDownloader(
          CanDoBackgroundDownload(update_context, size));
  crx_downloader->set_progress_callback(progress_callback);
  cancellation->OnCancel(crx_downloader->StartDownload(
      urls, hash,
      base::BindOnce(&DownloadComplete, crx_downloader, cancellation,
                     event_adder, std::move(callback))));
}

}  // namespace

base::OnceClosure DownloadOperation(
    scoped_refptr<const UpdateContext> update_context,
    const std::vector<GURL>& urls,
    int64_t size,
    const std::string& hash,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CrxDownloader::ProgressCallback progress_callback,
    base::OnceCallback<void(
        const base::expected<base::FilePath, CategorizedError>&)> callback) {
  auto cancellation = base::MakeRefCounted<Cancellation>();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
          [](base::RepeatingCallback<int64_t(const base::FilePath&)>
                 get_available_space) {
            base::ScopedTempDir temp_dir;
            return temp_dir.CreateUniqueTempDir()
                       ? get_available_space.Run(temp_dir.GetPath())
                       : int64_t{0};
          },
          update_context->get_available_space),
      base::BindOnce(&HandleAvailableSpace, update_context, cancellation, urls,
                     size, hash, progress_callback, event_adder,
                     std::move(callback)));
  return base::BindOnce(&Cancellation::Cancel, cancellation);
}

}  // namespace update_client
