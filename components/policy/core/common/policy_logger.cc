// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include <deque>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/common/features.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/channel/channel_info.h"
#endif

namespace policy {

namespace {

// The base format for the Chromium Code Search URLs.
constexpr char kChromiumCSUrlFormat[] =
    "https://source.chromium.org/chromium/chromium/src/+/main:%s;l=%i";

// The suffix format for the Chromium Code Search URLs with a specified change
// ID.
constexpr char kLastChangeSuffixFormat[] = ";drc:%s";

// The invalid last change value that is returned by `GetLastChange()` for local
// builds.
constexpr char kInvalidLastChange[] =
    "0000000000000000000000000000000000000000-"
    "0000000000000000000000000000000000000000";

// Gets the string value for the log source.
std::string GetLogSourceValue(const PolicyLogger::Log::Source log_source) {
  switch (log_source) {
    case PolicyLogger::Log::Source::kPolicyProcessing:
      return "Policy Processing";
    case PolicyLogger::Log::Source::kCBCMEnrollment:
      return "CBCM Enrollment";
    case PolicyLogger::Log::Source::kPlatformPolicy:
      return "Platform Policy";
    case PolicyLogger::Log::Source::kPolicyFetching:
      return "Policy Fetching";
    case PolicyLogger::Log::Source::kAuthentication:
      return "Authentication";
    case PolicyLogger::Log::Source::kRemoteCommands:
      return "Remote Commands";
    case PolicyLogger::Log::Source::kDeviceTrust:
      return "Device Trust";
    case PolicyLogger::Log::Source::kOidcEnrollment:
      return "OIDC Enrollment";
    case PolicyLogger::Log::Source::kExtensibleSSO:
      return "Extensible SSO";
    case PolicyLogger::Log::Source::kReporting:
      return "Reporting";
  }
}

std::string GetLogSeverity(const PolicyLogger::Log::Severity log_severity) {
  switch (log_severity) {
    case PolicyLogger::Log::Severity::kInfo:
      return "INFO";
    case PolicyLogger::Log::Severity::kWarning:
      return "WARNING";
    case PolicyLogger::Log::Severity::kError:
      return "ERROR";
    case PolicyLogger::Log::Severity::kVerbose:
      return "VERBOSE";
  }
}

int GetLogSeverityInt(const PolicyLogger::Log::Severity log_severity) {
  switch (log_severity) {
    case PolicyLogger::Log::Severity::kInfo:
      return ::logging::LOGGING_INFO;
    case PolicyLogger::Log::Severity::kWarning:
      return ::logging::LOGGING_WARNING;
    case PolicyLogger::Log::Severity::kError:
      return ::logging::LOGGING_ERROR;
    case PolicyLogger::Log::Severity::kVerbose:
      return ::logging::LOGGING_VERBOSE;
  }
}

// Most logging initializes `file` from __FILE__. Unfortunately, because we
// build from out/Foo we get a `../../` (or \) prefix for all of our
// __FILE__s. This isn't true for base::Location::Current() which already does
// the stripping (and is used for some logging, especially CHECKs).
//
// Here we strip the first 6 (../../ or ..\..\) characters if `file` starts
// with `.` but defensively clamp to strlen(file) just in case.
//
// TODO(nicolaso): Consider migrating to use base::Location directly. See
// base/check.h for inspiration.
std::string_view StripParentPrefix(std::string_view file) {
  return (!file.empty() && file[0] == '.')
             ? std::string_view(file).substr(
                   std::min(std::size_t{6}, file.length()))
             : file;
}

// Constructs the URL for Chromium Code Search that points to the line of code
// that generated the log and the Chromium git revision hash.
std::string GetLineURL(std::string_view file, int line) {
  std::string last_change(version_info::GetLastChange());

  std::string url =
      base::StringPrintf(kChromiumCSUrlFormat, StripParentPrefix(file), line);
  if (last_change != kInvalidLastChange) {
    // The substring separates the last change commit hash from the branch name
    // on the '-'.
    url += base::StringPrintf(
        kLastChangeSuffixFormat,
        last_change.substr(0, last_change.find('-')).c_str());
  }
  return url;
}

// GetFileBasename("/a/b/c.txt") -> "c.txt"
std::string_view GetFileBasename(std::string_view file) {
  size_t pos = file.find_last_of("/\\");
  return pos == std::string_view::npos ? file : file.substr(pos + 1);
}

std::string GetFileAndLine(std::string_view file, int line) {
  return base::StrCat({GetFileBasename(file), ":", base::NumberToString(line)});
}

std::string GetTimestampString(base::Time timestamp) {
  base::Time::Exploded exploded;
  timestamp.LocalExplode(&exploded);
  int hour12 = exploded.hour % 12;
  if (hour12 == 0) {
    hour12 = 12;
  }
  const char* ampm = exploded.hour >= 12 ? "PM" : "AM";
  return absl::StrFormat("%d-%02d-%02d %02d:%02d:%02d %s", exploded.year,
                         exploded.month, exploded.day_of_month, hour12,
                         exploded.minute, exploded.second, ampm);
}

void WriteU8(uint8_t value, std::string& buffer) {
  buffer.push_back(static_cast<char>(value));
}

void WriteU32(uint32_t value, std::string& buffer) {
  buffer.append(base::as_string_view(base::byte_span_from_ref(value)));
}

void WriteString(std::string_view str, std::string& buffer) {
  WriteU32(base::checked_cast<uint32_t>(str.size()), buffer);
  buffer.append(str);
}

bool ReadString(base::SpanReader<const uint8_t>& reader,
                std::string_view& str) {
  uint32_t size = 0;
  if (!reader.ReadU32NativeEndian(size)) {
    return false;
  }
  if (auto str_bytes = reader.Read(size)) {
    str = base::as_string_view(*str_bytes);
    return true;
  }
  return false;
}

void SerializeLog(const PolicyLogger::Log& log, std::string& buffer) {
  WriteU8(static_cast<uint8_t>(log.log_severity()), buffer);
  WriteU8(static_cast<uint8_t>(log.log_source()), buffer);
  // This is *unsigned*, so we don't have the 2038 problem. It'll work until Feb
  // 2106.
  WriteU32(
      base::checked_cast<uint32_t>(
          std::max<int64_t>(log.timestamp().InMillisecondsSinceUnixEpoch(), 0) /
          1000),
      buffer);
  WriteU32(base::checked_cast<uint32_t>(log.line()), buffer);
  WriteString(log.file(), buffer);
  WriteString(log.message(), buffer);
}

std::optional<PolicyLogger::Log> DeserializeLog(
    base::SpanReader<const uint8_t>& reader) {
  uint8_t severity_val = 0;
  uint8_t source_val = 0;
  uint32_t timestamp_sec = 0;
  uint32_t line_val = 0;
  std::string_view file;
  std::string_view message;

  if (!reader.ReadU8NativeEndian(severity_val) ||
      !reader.ReadU8NativeEndian(source_val) ||
      !reader.ReadU32NativeEndian(timestamp_sec) ||
      !reader.ReadU32NativeEndian(line_val) || !ReadString(reader, file) ||
      !ReadString(reader, message) ||
      severity_val >
          static_cast<uint8_t>(PolicyLogger::Log::Severity::kMaxValue) ||
      source_val > static_cast<uint8_t>(PolicyLogger::Log::Source::kMaxValue) ||
      !base::IsValueInRangeForNumericType<int>(line_val)) {
    return std::nullopt;
  }

  return PolicyLogger::Log(
      static_cast<PolicyLogger::Log::Severity>(severity_val),
      static_cast<PolicyLogger::Log::Source>(source_val), std::string(message),
      file, static_cast<int>(line_val),
      base::Time::FromMillisecondsSinceUnixEpoch(
          static_cast<int64_t>(timestamp_sec) * 1000));
}

}  // namespace

PolicyLogger::Log::Log(const Severity log_severity,
                       const Source log_source,
                       std::string message,
                       std::string_view file,
                       const int line,
                       base::Time timestamp)
    : log_severity_(log_severity),
      log_source_(log_source),
      message_(std::move(message)),
      file_(file),
      line_(line),
      timestamp_(timestamp) {}

PolicyLogger::Log::Log(const Log&) = default;
PolicyLogger::Log::Log(Log&&) = default;
PolicyLogger::Log& PolicyLogger::Log::operator=(Log&&) = default;
PolicyLogger::Log::~Log() = default;

// static
PolicyLogger* PolicyLogger::GetInstance() {
  static base::NoDestructor<PolicyLogger> instance;
  return instance.get();
}

// static
bool PolicyLogger::IsPolicyLoggingEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  // All choices are explicit to ensure that new channels added in the future
  // will need to be explicitly handled here and follow the right logic.
  switch (ash::GetChannel()) {
    case version_info::Channel::STABLE:
      return false;
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
    case version_info::Channel::UNKNOWN:
      return true;
  }
#else
  return true;
#endif
}

PolicyLogger::LogHelper::LogHelper(
    const LogType log_type,
    const PolicyLogger::Log::Severity log_severity,
    const int log_verbosity,
    const PolicyLogger::Log::Source log_source,
    std::string_view file,
    const int line)
    : log_type_(log_type),
      log_severity_(log_severity),
      log_verbosity_(log_verbosity),
      log_source_(log_source),
      file_(file),
      line_(line) {}

PolicyLogger::LogHelper::~LogHelper() {
  if (PolicyLogger::IsPolicyLoggingEnabled()) {
    PolicyLogger::GetInstance()->AddLog(PolicyLogger::Log(
        log_severity_, log_source_, message_buffer_.str(), file_, line_));
  }
  StreamLog();
}

void PolicyLogger::LogHelper::StreamLog() const {
#if !DCHECK_IS_ON()
  if (log_type_ == LogHelper::LogType::kDLog) {
    return;
  }
#else
  // Suppress a -Wunused-private-field warning.
  (void)log_type_;
#endif

  // Check for verbose logging.
  if (log_verbosity_ != policy::PolicyLogger::LogHelper::kNoVerboseLog) {
    LAZY_STREAM(
        ::logging::LogMessage(file_.data(), line_, -(log_verbosity_)).stream(),
        log_verbosity_ <=
            ::logging::GetVlogLevelHelper(file_.data(), file_.size()))
        << message_buffer_.str();
    return;
  }

  int log_severity_int = GetLogSeverityInt(log_severity_);

  LAZY_STREAM(
      ::logging::LogMessage(file_.data(), line_, log_severity_int).stream(),
      ::logging::ShouldCreateLogMessage(log_severity_int))
      << message_buffer_.str();
}

base::DictValue PolicyLogger::Log::GetAsDict() const {
  return base::DictValue()
      .Set("message", message_)
      .Set("logSeverity", GetLogSeverity(log_severity_))
      .Set("logSource", GetLogSourceValue(log_source_))
      .Set("fileAndLine", GetFileAndLine(file_, line_))
      .Set("location", GetLineURL(file_, line_))
      .Set("timestamp", GetTimestampString(timestamp_));
}

policy::mojom::LogPtr PolicyLogger::Log::GetAsMojoLog() const {
  return policy::mojom::Log::New(
      message_, GetLogSeverity(log_severity_), GetLogSourceValue(log_source_),
      GetFileAndLine(file_, line_), GetLineURL(file_, line_),
      GetTimestampString(timestamp_));
}

PolicyLogger::PolicyLogger() {
  DETACH_FROM_SEQUENCE(compression_sequence_checker_);
}

PolicyLogger::~PolicyLogger() = default;

void PolicyLogger::EnableLogCompression(
    scoped_refptr<base::SequencedTaskRunner> compression_task_runner) {
  CHECK(compression_task_runner);
  base::AutoLock lock(lock_);
  if (compression_task_runner_) {
    CHECK_IS_TEST();
    LOG(WARNING) << "PolicyLogger::EnableLogCompression called more than once";
    return;
  }
  compression_task_runner_ = std::move(compression_task_runner);
}

void PolicyLogger::AddLog(PolicyLogger::Log&& new_log) {
  base::AutoLock lock(lock_);
  logs_.emplace_back(std::move(new_log));
  if (compression_task_runner_) {
    if (logs_.size() >= kMaxUncompressedLogCount) {
      std::vector<Log> logs_to_flush(std::make_move_iterator(logs_.begin()),
                                     std::make_move_iterator(logs_.end()));
      logs_.clear();
      compression_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&PolicyLogger::CompressAndAppendLogs,
                         base::Unretained(this), std::move(logs_to_flush)));
    }
  } else if (logs_.size() > kMaxUncompressedLogCount) {
    logs_.pop_front();
  }
}

void PolicyLogger::GetLogs(
    base::OnceCallback<void(std::vector<Log>)> callback) {
  std::vector<Log> in_memory_logs;
  {
    base::AutoLock lock(lock_);
    in_memory_logs = std::vector<Log>(logs_.begin(), logs_.end());
    if (compression_task_runner_) {
      compression_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&PolicyLogger::DecompressAndReadLogs,
                         base::Unretained(this)),
          base::BindOnce(
              [](std::vector<Log> in_memory_logs,
                 base::OnceCallback<void(std::vector<Log>)> callback,
                 std::vector<Log> logs) {
                logs.insert(logs.end(),
                            std::make_move_iterator(in_memory_logs.begin()),
                            std::make_move_iterator(in_memory_logs.end()));
                std::move(callback).Run(std::move(logs));
              },
              std::move(in_memory_logs), std::move(callback)));
      return;
    }
  }
  std::move(callback).Run(std::move(in_memory_logs));
}

void PolicyLogger::CompressAndAppendLogs(std::vector<Log> logs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(compression_sequence_checker_);
  if (logs.empty()) {
    return;
  }
  std::vector<Log> all_logs = DecompressAndReadLogs();
  all_logs.insert(all_logs.end(), std::make_move_iterator(logs.begin()),
                  std::make_move_iterator(logs.end()));
  if (all_logs.size() > kMaxCompressedLogCount) {
    all_logs.erase(
        all_logs.begin(),
        all_logs.begin() + (all_logs.size() - kMaxCompressedLogCount));
  }
  std::string uncompressed_buffer;
  for (const auto& log : all_logs) {
    SerializeLog(log, uncompressed_buffer);
  }
  std::string new_compressed_buffer;
  if (!compression::GzipCompress(base::as_byte_span(uncompressed_buffer),
                                 &new_compressed_buffer)) {
    return;
  }
  compressed_buffer_size_.store(new_compressed_buffer.size(),
                                std::memory_order_relaxed);
  compressed_log_count_.store(all_logs.size(), std::memory_order_relaxed);
  compressed_buffer_ = std::move(new_compressed_buffer);
}

std::vector<PolicyLogger::Log> PolicyLogger::DecompressAndReadLogs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(compression_sequence_checker_);
  std::vector<Log> logs;
  if (compressed_buffer_.empty()) {
    return logs;
  }
  std::string uncompressed_buffer;
  if (!compression::GzipUncompress(base::as_byte_span(compressed_buffer_),
                                   &uncompressed_buffer)) {
    return logs;
  }
  base::SpanReader<const uint8_t> reader(
      base::as_byte_span(uncompressed_buffer));
  while (std::optional<Log> log = DeserializeLog(reader)) {
    logs.push_back(std::move(*log));
  }
  return logs;
}

void PolicyLogger::GetAsList(GetAsListCallback callback) {
  GetLogs(base::BindOnce(
      [](GetAsListCallback callback, std::vector<Log> logs) {
        base::ListValue all_logs_list;
        all_logs_list.reserve(logs.size());
        for (const Log& log : logs) {
          all_logs_list.Append(log.GetAsDict());
        }
        std::move(callback).Run(std::move(all_logs_list));
      },
      std::move(callback)));
}

void PolicyLogger::GetAsMojoList(GetAsMojoListCallback callback) {
  GetLogs(base::BindOnce(
      [](GetAsMojoListCallback callback, std::vector<Log> logs) {
        std::vector<policy::mojom::LogPtr> all_logs_list;
        all_logs_list.reserve(logs.size());
        std::ranges::transform(logs, std::back_inserter(all_logs_list),
                               &PolicyLogger::Log::GetAsMojoLog);
        std::move(callback).Run(std::move(all_logs_list));
      },
      std::move(callback)));
}

void PolicyLogger::RecordPerformanceMetrics() {
  size_t memory_usage = 0;
  size_t log_count = 0;
  {
    base::AutoLock lock(lock_);
    for (const auto& log : logs_) {
      memory_usage += sizeof(Log) + log.message().size() + log.file().size();
    }
    log_count = logs_.size();
  }
  base::UmaHistogramCounts1M("Enterprise.PolicyLogger.MemoryUsage.Uncompressed",
                             memory_usage);
  base::UmaHistogramCounts10000("Enterprise.PolicyLogger.LogCount.Uncompressed",
                                log_count);
  base::UmaHistogramCounts1M(
      "Enterprise.PolicyLogger.MemoryUsage.Compressed",
      compressed_buffer_size_.load(std::memory_order_relaxed));
  base::UmaHistogramCounts10000(
      "Enterprise.PolicyLogger.LogCount.Compressed",
      compressed_log_count_.load(std::memory_order_relaxed));
}

void PolicyLogger::ResetLoggerForTesting() {
  CHECK_IS_TEST();
  base::AutoLock lock(lock_);
  DETACH_FROM_SEQUENCE(compression_sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(compression_sequence_checker_);
  logs_.clear();
  compressed_buffer_.clear();
  compressed_buffer_size_.store(0, std::memory_order_relaxed);
  compressed_log_count_.store(0, std::memory_order_relaxed);
  compression_task_runner_.reset();
  DETACH_FROM_SEQUENCE(compression_sequence_checker_);
}

}  // namespace policy
