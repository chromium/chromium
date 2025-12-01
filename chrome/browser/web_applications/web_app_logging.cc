// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_logging.h"

#include <string>
#include <string_view>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/to_string.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"

namespace web_app {
namespace {
// Heuristic from looking at a number of log entries and averaging their byte
// size.
static constexpr int kEstimatedBytesPerLogEntry = 1 * 1024;  // 2KB
// The file size should be large enough that it will not need to rotate often,
// but small enough that it will not get too expensive to re-serialize every
// time the log is flushed.
static constexpr int kMaxLogFileSizeBytes = 2 * 1024 * 1024;  // 2MB
static constexpr base::TimeDelta kLogWriteDelay = base::Seconds(5);
// When rotating log files, there can be multiple files that rotate on the same
// date. To create a unique file name, we add an index to the end of the file
// name. This is the maximum index that will be used.
static constexpr int kMaxRotatedLogFileIndex = 100;

int g_max_log_file_size_bytes = kMaxLogFileSizeBytes;

// This task runner is used to read or write log files to disk.
base::LazyThreadPoolSequencedTaskRunner g_log_writing_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
base::LazyThreadPoolSequencedTaskRunner g_log_deletion_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits({base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                          base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

bool IsEmptyIconBitmapsForIconUrl(const IconsMap& icons_map,
                                  const GURL& icon_url) {
  IconsMap::const_iterator iter = icons_map.find(icon_url);
  if (iter == icons_map.end())
    return true;

  const std::vector<SkBitmap>& icon_bitmaps = iter->second;
  if (icon_bitmaps.empty())
    return true;

  for (const SkBitmap& icon_bitmap : icon_bitmaps) {
    if (!icon_bitmap.isNull() && !icon_bitmap.drawsNothing())
      return false;
  }

  return true;
}

void WritePersistableLogBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                 const base::FilePath& file_path,
                                 base::Value error_log) {
  if (!utils->CreateDirectory(file_path.DirName())) {
    DLOG(ERROR) << "Could not create directory: " << file_path.DirName();
    return;
  }
  if (!utils->WriteFile(file_path,
                        base::as_byte_span(error_log.DebugString()))) {
    DLOG(ERROR) << "Could not write log file: " << file_path;
  }
}

std::optional<base::ListValue> ReadLogFileBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& log_file) {
  if (!utils->PathExists(log_file)) {
    return std::nullopt;
  }
  std::string contents;
  if (!utils->ReadFileToString(log_file, &contents)) {
    return std::nullopt;
  }
  return base::JSONReader::ReadList(
      contents, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
}

void RotateLogFileBlocking(scoped_refptr<FileUtilsWrapper> utils,
                           const base::FilePath& current_log_file) {
  if (!utils->PathExists(current_log_file)) {
    DLOG(ERROR) << "Current log file does not exist: " << current_log_file;
    return;
  }
  std::optional<base::FilePath> rotated_filename;
  std::string date_str =
      base::UnlocalizedTimeFormatWithPattern(base::Time::Now(), "y-MM-dd");
  for (int free_index = -1; free_index < kMaxRotatedLogFileIndex;
       ++free_index) {
    std::string index_str =
        free_index >= 0 ? base::NumberToString(free_index) : "";
    base::FilePath candidate_filename =
        current_log_file.InsertBeforeExtensionASCII(
            base::StrCat({date_str, ".", index_str}));
    if (!utils->PathExists(candidate_filename)) {
      rotated_filename = candidate_filename;
      break;
    }
  }
  if (!rotated_filename.has_value()) {
    DLOG(ERROR) << "Could not find a free index for rotated log file.";
    return;
  }
  if (!utils->Move(current_log_file, *rotated_filename)) {
    LOG(ERROR) << "Could not move log file: " << current_log_file.value()
               << " to " << rotated_filename.value();
  }
}

}  // namespace

// static
base::FilePath PersistableLog::GetLogPath(Profile* profile,
                                          std::string_view log_filename) {
  return GetWebAppsRootDirectory(profile).AppendASCII("Logs").AppendASCII(
      log_filename);
}
// static
int PersistableLog::GetMaxInMemoryLogEntries() {
  static constexpr int kMaxInMemoryLogEntriesWithDebugInfoEnabled = 1000;
  static constexpr int kMaxInMemoryLogEntriesWithDebugInfoDisabled = 20;
  return base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)
             ? kMaxInMemoryLogEntriesWithDebugInfoEnabled
             : kMaxInMemoryLogEntriesWithDebugInfoDisabled;
}

// static
PersistableLogMode PersistableLog::GetMode() {
  return base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)
             ? PersistableLogMode::kPersistToDisk
             : PersistableLogMode::kInMemory;
}

// static
base::AutoReset<int> PersistableLog::SetMaxLogFileSizeBytesForTesting(
    int size) {
  CHECK_IS_TEST();
  return base::AutoReset<int>(&g_max_log_file_size_bytes, size);
}

PersistableLog::PersistableLog(const base::FilePath& log_file,
                               PersistableLogMode mode,
                               int max_log_entries_in_memory,
                               scoped_refptr<FileUtilsWrapper> file_utils)
    : log_file_(log_file),
      mode_(mode),
      file_utils_(std::move(file_utils)),
      max_log_entries_in_memory_(max_log_entries_in_memory),
      log_write_timer_(FROM_HERE,
                       kLogWriteDelay,
                       this,
                       &PersistableLog::MaybeWriteCurrentLog) {
  CHECK(!log_file_.empty());
  CHECK(file_utils_);
  CHECK(log_file_.IsAbsolute());
  CHECK(!log_file_.Extension().empty());

  switch (mode) {
    case PersistableLogMode::kInMemory:
      // For in-memory, we only do one file-related task here to delete old
      // logs.
      // TODO(crbug.com/40224498): Clean up logs older than X days on startup.
      g_log_deletion_task_runner.Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(&FileUtilsWrapper::DeleteFileRecursively),
              file_utils_, log_file_.DirName()));
      // Signal the load complete event so that Append() can add to the
      // in-memory log.
      on_load_complete_.Signal();
      return;
    case PersistableLogMode::kPersistToDisk:
      g_log_writing_task_runner.Get()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ReadLogFileBlocking, file_utils_, log_file_),
          base::BindOnce(&PersistableLog::OnLatestLogLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
  }
}

PersistableLog::~PersistableLog() {
  MaybeWriteCurrentLog();
}

void PersistableLog::Append(base::Value object) {
#if DCHECK_IS_ON()
  // This is wrapped with DCHECK_IS_ON() to prevent calling DebugString() in
  // production builds.
  DVLOG(1) << (on_load_complete_.is_signaled() ? "" : "[LOADING] ")
           << log_file_.BaseName() << ": " << object.DebugString();
#endif
  if (!on_load_complete_.is_signaled()) {
    on_load_complete_.Post(
        FROM_HERE,
        base::BindOnce(&PersistableLog::Append, weak_ptr_factory_.GetWeakPtr(),
                       std::move(object)));
    return;
  }
  log_.push_front(object.Clone());
  if (base::saturated_cast<int>(log_.size()) > max_log_entries_in_memory_) {
    log_.resize(max_log_entries_in_memory_);
  }
  current_log_for_disk_.Append(std::move(object));
  if (mode_ == PersistableLogMode::kPersistToDisk) {
    log_write_timer_.Reset();
    // Technically we can log frequently for a while and never write to disk, as
    // the timer won't trigger. To mitigate, flush if we reach 2x the max log
    // size.
    if (base::saturated_cast<int>(current_log_for_disk_.size()) *
            kEstimatedBytesPerLogEntry * 2 >=
        g_max_log_file_size_bytes) {
      MaybeWriteCurrentLog();
    }
  }
}

const base::circular_deque<base::Value>& PersistableLog::GetEntries() const {
  return log_;
}

void PersistableLog::WaitForLoadAndFlushForTesting(
    base::OnceClosure done) const {
  // First wait for the log to load, and call this method again.
  if (!on_load_complete_.is_signaled()) {
    on_load_complete_.Post(
        FROM_HERE,
        base::BindOnce(&PersistableLog::WaitForLoadAndFlushForTesting,
                       weak_ptr_factory_.GetWeakPtr(), std::move(done)));
    return;
  }
  // Const_cast seems like the least-disruptive way to allow tests to flush
  // the log to disk without adding a way for tests to access a non-const
  // version of the log.
  const_cast<PersistableLog*>(this)->MaybeWriteCurrentLog();
  // Post a task to the log writing sequence to ensure that the log is written
  // before running the callback.
  base::RepeatingClosure barrier = base::BarrierClosure(2, std::move(done));
  g_log_writing_task_runner.Get()->PostTaskAndReply(FROM_HERE,
                                                    base::DoNothing(), barrier);
  g_log_deletion_task_runner.Get()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), barrier);
}

void PersistableLog::OnLatestLogLoaded(
    std::optional<base::ListValue> loaded_log) {
  if (loaded_log.has_value()) {
    current_log_for_disk_ = loaded_log->Clone();
    // Note: Items are added to the front as the in-memory log order is from
    // newest to oldest.
    log_.reserve(current_log_for_disk_.size());
    for (base::Value& value : current_log_for_disk_) {
      log_.push_front(value.Clone());
    }
  }
  on_load_complete_.Signal();
}

void PersistableLog::MaybeWriteCurrentLog() {
  if (mode_ != PersistableLogMode::kPersistToDisk) {
    return;
  }

  // The log is written, in full, every time this method is called. This is a
  // tradeoff between using an easy-to-read and easy-to-write format like JSON,
  // which requires re-serialization fully every write (as you cannot simply
  // easily append with our built-in methods), and the cost of rewriting the
  // whole file. Since this is only used for debugging for users who have
  // enabled the debug info flag, the costs here seem acceptable, especially
  // since the total log size is limited, and we only write 5 seconds after the
  // last log entry is added.
  base::OnceClosure log_write_task =
      base::BindOnce(&WritePersistableLogBlocking, file_utils_, log_file_,
                     base::Value(current_log_for_disk_.Clone()));
  // Possibly do a log rotation.
  if (current_disk_log_entry_count_ * kEstimatedBytesPerLogEntry >
      g_max_log_file_size_bytes) {
    log_write_task =
        base::BindOnce(&RotateLogFileBlocking, file_utils_, log_file_)
            .Then(std::move(log_write_task));
    current_log_for_disk_.clear();
  }

  g_log_writing_task_runner.Get()->PostTask(FROM_HERE,
                                            std::move(log_write_task));
  current_disk_log_entry_count_ = current_log_for_disk_.size();
}

InstallErrorLogEntry::InstallErrorLogEntry(
    bool background_installation,
    webapps::WebappInstallSource install_surface)
    : background_installation_(background_installation),
      install_surface_(install_surface) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    error_dict_ = std::make_unique<base::Value::Dict>();
  }
}

InstallErrorLogEntry::~InstallErrorLogEntry() = default;

bool InstallErrorLogEntry::HasErrorDict() const {
  return error_dict_ && !error_dict_->empty();
}

base::Value InstallErrorLogEntry::TakeErrorDict() {
  DCHECK(error_dict_);
  base::Value::Dict error_dict = std::move(*error_dict_);
  *error_dict_ = base::Value::Dict();
  return base::Value(std::move(error_dict));
}

void InstallErrorLogEntry::LogUrlLoaderError(
    const char* stage,
    const std::string& url,
    webapps::WebAppUrlLoaderResult result) {
  if (!error_dict_)
    return;

  base::Value::Dict url_loader_error;

  url_loader_error.Set("WebAppUrlLoader::Result", base::ToString(result));

  LogErrorObject(stage, url, std::move(url_loader_error));
}

void InstallErrorLogEntry::LogExpectedAppIdError(
    const char* stage,
    const std::string& url,
    const webapps::AppId& app_id,
    const webapps::AppId& expected_app_id) {
  if (!error_dict_)
    return;

  base::Value::Dict expected_app_id_error;
  expected_app_id_error.Set("expected_app_id", expected_app_id);
  expected_app_id_error.Set("app_id", app_id);

  LogErrorObject(stage, url, std::move(expected_app_id_error));
}

void InstallErrorLogEntry::LogDownloadedIconsErrors(
    const WebAppInstallInfo& web_app_info,
    IconsDownloadedResult icons_downloaded_result,
    const IconsMap& icons_map,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (!error_dict_)
    return;

  base::Value::Dict icon_errors;
  {
    // Reports errors only, omits successful entries.
    base::Value::List icons_http_errors;

    for (const auto& url_and_http_code : icons_http_results) {
      const GURL& icon_url = url_and_http_code.first.url;
      int http_status_code = url_and_http_code.second;
      const char* http_code_desc = net::GetHttpReasonPhrase(
          static_cast<net::HttpStatusCode>(http_status_code));

      // If the SkBitmap for`icon_url` is missing in `icons_map` then we report
      // this miss as an error, even for net::HttpStatusCode::HTTP_OK.
      if (IsEmptyIconBitmapsForIconUrl(icons_map, icon_url)) {
        base::Value::Dict icon_http_error;

        icon_http_error.Set("icon_url", icon_url.spec());
        icon_http_error.Set("icon_size",
                            url_and_http_code.first.size.ToString());
        icon_http_error.Set("http_status_code", http_status_code);
        icon_http_error.Set("http_code_desc", http_code_desc);

        icons_http_errors.Append(std::move(icon_http_error));
      }
    }

    if (icons_downloaded_result != IconsDownloadedResult::kCompleted ||
        !icons_http_errors.empty()) {
      icon_errors.Set("icons_downloaded_result",
                      IconsDownloadedResultToString(icons_downloaded_result));
    }

    if (!icons_http_errors.empty())
      icon_errors.Set("icons_http_results", std::move(icons_http_errors));
  }

  if (web_app_info.is_generated_icon)
    icon_errors.Set("is_generated_icon", true);

  if (!icon_errors.empty()) {
    LogErrorObject("OnIconsRetrieved", web_app_info.start_url().spec(),
                   std::move(icon_errors));
  }
}

void InstallErrorLogEntry::LogHeaderIfLogEmpty(const std::string& url) {
  if (!error_dict_ || !error_dict_->empty())
    return;

  error_dict_->Set("!url", url);
  error_dict_->Set("install_surface", static_cast<int>(install_surface_));
  error_dict_->Set("background_installation", background_installation_);
  error_dict_->Set("stages", base::Value::List());
}

void InstallErrorLogEntry::LogErrorObject(const char* stage,
                                          const std::string& url,
                                          base::Value::Dict object) {
  if (!error_dict_)
    return;

  LogHeaderIfLogEmpty(url);

  object.Set("!stage", stage);
  error_dict_->FindList("stages")->Append(std::move(object));
}

}  // namespace web_app
