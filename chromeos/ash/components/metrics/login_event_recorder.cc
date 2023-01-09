// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/metrics/login_event_recorder.h"

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"

namespace ash {

namespace {

constexpr char kUptime[] = "uptime";
constexpr char kDisk[] = "disk";

// The pointer to this object is used as a perfetto async event id.
constexpr char kBootTimes[] = "BootTimes";

#define FPL(value) FILE_PATH_LITERAL(value)

// Dir uptime & disk logs are located in.
constexpr const base::FilePath::CharType kLogPath[] = FPL("/tmp");

// Dir log{in,out} logs are located in.
constexpr base::FilePath::CharType kLoginLogPath[] = FPL("/home/chronos/user");

// Prefix for the time measurement files.
constexpr base::FilePath::CharType kUptimePrefix[] = FPL("uptime-");

// Prefix for the disk usage files.
constexpr base::FilePath::CharType kDiskPrefix[] = FPL("disk-");

// Prefix and suffix for the "stats saved" flags file.
constexpr base::FilePath::CharType kStatsPrefix[] = FPL("stats-");
constexpr base::FilePath::CharType kWrittenSuffix[] = FPL(".written");

// Names of login stats files.
constexpr base::FilePath::CharType kLoginSuccess[] = FPL("login-success");

// The login times will be written immediately when the login animation ends,
// and this is used to ensure the data is always written if this amount is
// elapsed after login.
constexpr int64_t kLoginTimeWriteDelayMs = 20000;

// Appends the given buffer into the file. Returns the number of bytes
// written, or -1 on error.<
// TODO(satorux): Move this to file_util.
int AppendFile(const base::FilePath& file_path, const char* data, int size) {
  // Appending boot times to (probably) a symlink in /tmp is a security risk for
  // developers with chromeos=1 builds.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return -1;

  FILE* file = base::OpenFile(file_path, "a");
  if (!file)
    return -1;

  const int num_bytes_written = fwrite(data, 1, size, file);
  base::CloseFile(file);
  return num_bytes_written;
}

void WriteTimes(const std::string base_name,
                const std::string uma_name,
                const std::string uma_prefix,
                std::vector<LoginEventRecorder::TimeMarker> times) {
  DCHECK(times.size());
  const int kMinTimeMillis = 1;
  const int kMaxTimeMillis = 30000;
  const int kNumBuckets = 100;
  const base::FilePath log_path(kLoginLogPath);

  // Need to sort by time since the entries may have been pushed onto the
  // vector (on the UI thread) in a different order from which they were
  // created (potentially on other threads).
  std::sort(times.begin(), times.end());

  base::TimeTicks first = times.front().time();
  base::TimeTicks last = times.back().time();
  base::TimeDelta total = last - first;
  base::HistogramBase* total_hist = base::Histogram::FactoryTimeGet(
      uma_name, base::Milliseconds(kMinTimeMillis),
      base::Milliseconds(kMaxTimeMillis), kNumBuckets,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  total_hist->AddTime(total);
  std::string output =
      base::StringPrintf("%s: %.2f", uma_name.c_str(), total.InSecondsF());
  if (uma_name == "BootTime.Login2" || uma_name == "BootTime.LoginNewUser") {
    UMA_HISTOGRAM_CUSTOM_TIMES("Ash.Tast.BootTime.Login2", total,
                               base::Milliseconds(1), base::Seconds(300), 100);
  }
  base::TimeTicks prev = first;
  // Send first event to name the track:
  // "In Chrome, we usually don't bother setting explicit track names. If none
  // is provided, the track is named after the first event on the track."
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "startup", kBootTimes, TRACE_ID_LOCAL(kBootTimes), prev);

  for (unsigned int i = 0; i < times.size(); ++i) {
    const LoginEventRecorder::TimeMarker& tm = times[i];

    if (tm.url().has_value()) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
          "startup", tm.name(), TRACE_ID_LOCAL(kBootTimes), prev, "url",
          *tm.url());
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
          "startup", tm.name(), TRACE_ID_LOCAL(kBootTimes), tm.time(), "url",
          *tm.url());
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "startup", tm.name(), TRACE_ID_LOCAL(kBootTimes), prev);
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
          "startup", tm.name(), TRACE_ID_LOCAL(kBootTimes), tm.time());
    }

    base::TimeDelta since_first = tm.time() - first;
    base::TimeDelta since_prev = tm.time() - prev;
    std::string name;

    if (tm.send_to_uma()) {
      name = uma_prefix + tm.name();
      base::HistogramBase* prev_hist = base::Histogram::FactoryTimeGet(
          name, base::Milliseconds(kMinTimeMillis),
          base::Milliseconds(kMaxTimeMillis), kNumBuckets,
          base::HistogramBase::kUmaTargetedHistogramFlag);
      prev_hist->AddTime(since_prev);
    } else {
      name = tm.name();
    }
    if (tm.write_to_file()) {
      output += base::StringPrintf("\n%.2f +%.4f %s", since_first.InSecondsF(),
                                   since_prev.InSecondsF(), name.data());
      if (tm.url().has_value()) {
        output += ": ";
        output += *tm.url();
      }
    }
    prev = tm.time();
  }
  output += '\n';
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "startup", kBootTimes, TRACE_ID_LOCAL(kBootTimes), prev);

  base::WriteFile(log_path.Append(base_name), output.data(), output.size());
}

}  // namespace

LoginEventRecorder::TimeMarker::TimeMarker(const char* name,
                                           absl::optional<std::string> url,
                                           bool send_to_uma,
                                           bool write_to_file)
    : name_(name),
      url_(url),
      send_to_uma_(send_to_uma),
      write_to_file_(write_to_file) {}

LoginEventRecorder::TimeMarker::TimeMarker(const TimeMarker& other) = default;

LoginEventRecorder::TimeMarker::~TimeMarker() = default;

// static
LoginEventRecorder::Stats LoginEventRecorder::Stats::GetCurrentStats() {
  const base::FilePath kProcUptime(FPL("/proc/uptime"));
  const base::FilePath kDiskStat(FPL("/sys/block/sda/stat"));
  Stats stats;
  // Callers of this method expect synchronous behavior.
  // It's safe to allow IO here, because only virtual FS are accessed.
  base::ScopedAllowBlocking allow_blocking;
  base::ReadFileToString(kProcUptime, &stats.uptime_);
  base::ReadFileToString(kDiskStat, &stats.disk_);
  return stats;
}

std::string LoginEventRecorder::Stats::SerializeToString() const {
  if (uptime_.empty() && disk_.empty())
    return std::string();
  base::Value::Dict dictionary;
  dictionary.Set(kUptime, uptime_);
  dictionary.Set(kDisk, disk_);

  std::string result;
  if (!base::JSONWriter::Write(dictionary, &result)) {
    LOG(WARNING) << "LoginEventRecorder::Stats::SerializeToString(): failed.";
    return std::string();
  }

  return result;
}

// static
LoginEventRecorder::Stats LoginEventRecorder::Stats::DeserializeFromString(
    const std::string& source) {
  if (source.empty())
    return Stats();

  absl::optional<base::Value> maybe_value = base::JSONReader::Read(source);
  if (!maybe_value || !maybe_value->is_dict()) {
    LOG(ERROR) << "LoginEventRecorder::Stats::DeserializeFromString(): not a "
                  "dictionary: '"
               << source << "'";
    return Stats();
  }

  auto* uptime = maybe_value->GetDict().FindString(kUptime);
  auto* disk = maybe_value->GetDict().FindString(kDisk);
  if (!uptime || !disk) {
    LOG(ERROR)
        << "LoginEventRecorder::Stats::DeserializeFromString(): format error: '"
        << source << "'";
    return Stats();
  }

  Stats stats;
  stats.uptime_ = *uptime;
  stats.disk_ = *disk;
  return stats;
}

bool LoginEventRecorder::Stats::UptimeDouble(double* result) const {
  std::string uptime = uptime_;
  const size_t space_at = uptime.find_first_of(' ');
  if (space_at == std::string::npos)
    return false;

  uptime.resize(space_at);

  if (base::StringToDouble(uptime, result))
    return true;

  return false;
}

void LoginEventRecorder::Stats::RecordStats(const std::string& name,
                                            bool write_flag_file) const {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoginEventRecorder::Stats::RecordStatsAsync,
                     base::Owned(new Stats(*this)), name, write_flag_file));
}

void LoginEventRecorder::Stats::RecordStatsWithCallback(
    const std::string& name,
    bool write_flag_file,
    base::OnceClosure callback) const {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoginEventRecorder::Stats::RecordStatsAsync,
                     base::Owned(new Stats(*this)), name, write_flag_file),
      std::move(callback));
}

void LoginEventRecorder::Stats::RecordStatsAsync(
    const base::FilePath::StringType& name,
    bool write_flag_file) const {
  const base::FilePath log_path(kLogPath);
  const base::FilePath uptime_output =
      log_path.Append(base::FilePath(kUptimePrefix + name));
  const base::FilePath disk_output =
      log_path.Append(base::FilePath(kDiskPrefix + name));

  // Append numbers to the files.
  AppendFile(uptime_output, uptime_.data(), uptime_.size());
  AppendFile(disk_output, disk_.data(), disk_.size());
  if (write_flag_file) {
    const base::FilePath flag_path =
        log_path.Append(base::FilePath(kStatsPrefix + name + kWrittenSuffix));
    AppendFile(flag_path, "", 0);
  }
}

static base::LazyInstance<LoginEventRecorder>::DestructorAtExit
    g_login_event_recorder = LAZY_INSTANCE_INITIALIZER;

LoginEventRecorder::LoginEventRecorder()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(base::CurrentUIThread::IsSet());
  login_time_markers_.reserve(30);
  logout_time_markers_.reserve(30);

  // Remember login events for later retrieval by tests.
  constexpr char kKeepLoginEventsForTesting[] = "keep-login-events-for-testing";
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kKeepLoginEventsForTesting)) {
    PrepareEventCollectionForTesting();  // IN-TEST
  }
}

LoginEventRecorder::~LoginEventRecorder() = default;

// static
LoginEventRecorder* LoginEventRecorder::Get() {
  return g_login_event_recorder.Pointer();
}

void LoginEventRecorder::AddLoginTimeMarker(const char* marker_name,
                                            bool send_to_uma,
                                            bool write_to_file) {
  AddLoginTimeMarkerWithURL(marker_name, absl::optional<std::string>(),
                            send_to_uma, write_to_file);
}

void LoginEventRecorder::AddLoginTimeMarkerWithURL(
    const char* marker_name,
    absl::optional<std::string> url,
    bool send_to_uma,
    bool write_to_file) {
  AddMarker(&login_time_markers_,
            TimeMarker(marker_name, url, send_to_uma, write_to_file));
  // Store a copy for testing.
  if (login_time_markers_for_testing_.has_value()) {
    login_time_markers_for_testing_.value().push_back(
        login_time_markers_.back());
  }
}

void LoginEventRecorder::AddLogoutTimeMarker(const char* marker_name,
                                             bool send_to_uma) {
  AddMarker(&logout_time_markers_,
            TimeMarker(marker_name, absl::optional<std::string>(), send_to_uma,
                       /*write_to_file=*/true));
}

void LoginEventRecorder::RecordAuthenticationSuccess() {
  AddLoginTimeMarker("Authenticate", true);
  RecordCurrentStats(kLoginSuccess);
}

void LoginEventRecorder::RecordAuthenticationFailure() {
  // no nothing for now.
}

void LoginEventRecorder::RecordCurrentStats(const std::string& name) {
  Stats::GetCurrentStats().RecordStats(name, /*write_flag_file=*/false);
}

void LoginEventRecorder::ClearLoginTimeMarkers() {
  login_time_markers_.clear();
}

void LoginEventRecorder::ScheduleWriteLoginTimes(const std::string base_name,
                                                 const std::string uma_name,
                                                 const std::string uma_prefix) {
  callback_ = base::BindOnce(&WriteTimes, base_name, uma_name, uma_prefix);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LoginEventRecorder::WriteLoginTimesDelayed,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kLoginTimeWriteDelayMs));
}

void LoginEventRecorder::RunScheduledWriteLoginTimes() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (callback_.is_null())
    return;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(std::move(callback_), std::move(login_time_markers_)));
}

void LoginEventRecorder::WriteLogoutTimes(const std::string base_name,
                                          const std::string uma_name,
                                          const std::string uma_prefix) {
  WriteTimes(base_name, uma_name, uma_prefix, std::move(logout_time_markers_));
}

void LoginEventRecorder::PrepareEventCollectionForTesting() {
  if (login_time_markers_for_testing_.has_value())
    return;

  login_time_markers_for_testing_ = login_time_markers_;
}

const std::vector<LoginEventRecorder::TimeMarker>&
LoginEventRecorder::GetCollectedLoginEventsForTesting() {
  PrepareEventCollectionForTesting();  // IN-TEST
  return login_time_markers_for_testing_.value();
}

void LoginEventRecorder::AddMarker(std::vector<TimeMarker>* vector,
                                   TimeMarker&& marker) {
  // The marker vectors can only be safely manipulated on the main thread.
  // If we're late in the process of shutting down (eg. as can be the case at
  // logout), then we have to assume we're on the main thread already.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    vector->push_back(marker);
  } else {
    // Add the marker on the UI thread.
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&LoginEventRecorder::AddMarker,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          base::Unretained(vector), marker));
  }
}

void LoginEventRecorder::WriteLoginTimesDelayed() {
  if (callback_.is_null())
    return;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(std::move(callback_), std::move(login_time_markers_)));
}

}  // namespace ash
