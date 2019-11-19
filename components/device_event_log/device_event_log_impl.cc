// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_event_log/device_event_log_impl.h"

#include <cmath>
#include <list>
#include <set>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace device_event_log {

namespace {

const char* kLogLevelName[] = {"Error", "User", "Event", "Debug"};

const char* kLogTypeNetworkDesc = "Network";
const char* kLogTypePowerDesc = "Power";
const char* kLogTypeLoginDesc = "Login";
const char* kLogTypeBluetoothDesc = "Bluetooth";
const char* kLogTypeUsbDesc = "USB";
const char* kLogTypeHidDesc = "HID";
const char* kLogTypeMemoryDesc = "Memory";
const char* kLogTypePrinterDesc = "Printer";
const char* kLogTypeFidoDesc = "FIDO";

std::string GetLogTypeString(LogType type) {
  switch (type) {
    case LOG_TYPE_NETWORK:
      return kLogTypeNetworkDesc;
    case LOG_TYPE_POWER:
      return kLogTypePowerDesc;
    case LOG_TYPE_LOGIN:
      return kLogTypeLoginDesc;
    case LOG_TYPE_BLUETOOTH:
      return kLogTypeBluetoothDesc;
    case LOG_TYPE_USB:
      return kLogTypeUsbDesc;
    case LOG_TYPE_HID:
      return kLogTypeHidDesc;
    case LOG_TYPE_MEMORY:
      return kLogTypeMemoryDesc;
    case LOG_TYPE_PRINTER:
      return kLogTypePrinterDesc;
    case LOG_TYPE_FIDO:
      return kLogTypeFidoDesc;
    case LOG_TYPE_UNKNOWN:
      break;
  }
  NOTREACHED();
  return "Unknown";
}

LogType GetLogTypeFromString(const std::string& desc) {
  std::string desc_lc = base::ToLowerASCII(desc);
  for (int i = 0; i < LOG_TYPE_UNKNOWN; ++i) {
    auto type = static_cast<LogType>(i);
    std::string log_desc_lc = base::ToLowerASCII(GetLogTypeString(type));
    if (desc_lc == log_desc_lc)
      return type;
  }
  NOTREACHED() << "Unrecogized LogType: " << desc;
  return LOG_TYPE_UNKNOWN;
}

std::string DateAndTimeWithMicroseconds(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  // base::Time::Exploded does not include microseconds, but sometimes we need
  // microseconds, so append '.' + usecs to the end of the formatted string.
  int usecs = static_cast<int>(fmod(time.ToDoubleT() * 1000000, 1000000));
  return base::StringPrintf("%04d/%02d/%02d %02d:%02d:%02d.%06d", exploded.year,
                            exploded.month, exploded.day_of_month,
                            exploded.hour, exploded.minute, exploded.second,
                            usecs);
}

std::string TimeWithSeconds(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%02d:%02d:%02d", exploded.hour, exploded.minute,
                            exploded.second);
}

std::string TimeWithMillieconds(const base::Time& time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%02d:%02d:%02d.%03d", exploded.hour,
                            exploded.minute, exploded.second,
                            exploded.millisecond);
}

std::string LogEntryToString(const DeviceEventLogImpl::LogEntry& log_entry,
                             bool show_time,
                             bool show_file,
                             bool show_type,
                             bool show_level) {
  std::string line;
  if (show_time)
    line += "[" + TimeWithMillieconds(log_entry.time) + "] ";
  if (show_type)
    line += GetLogTypeString(log_entry.log_type) + ": ";
  if (show_level) {
    const char* kLevelDesc[] = {"ERROR", "USER", "EVENT", "DEBUG"};
    line += base::StringPrintf("%s: ", kLevelDesc[log_entry.log_level]);
  }
  if (show_file) {
    line += base::StringPrintf("%s:%d ", log_entry.file.c_str(),
                               log_entry.file_line);
  }
  line += log_entry.event;
  if (log_entry.count > 1)
    line += base::StringPrintf(" (%d)", log_entry.count);
  return line;
}

void LogEntryToDictionary(const DeviceEventLogImpl::LogEntry& log_entry,
                          base::DictionaryValue* output) {
  output->SetString("timestamp", DateAndTimeWithMicroseconds(log_entry.time));
  output->SetString("timestampshort", TimeWithSeconds(log_entry.time));
  output->SetString("level", kLogLevelName[log_entry.log_level]);
  output->SetString("type", GetLogTypeString(log_entry.log_type));
  output->SetString("file", base::StringPrintf("%s:%d ", log_entry.file.c_str(),
                                               log_entry.file_line));
  output->SetString("event", log_entry.event);
}

std::string LogEntryAsJSON(const DeviceEventLogImpl::LogEntry& log_entry) {
  base::DictionaryValue entry_dict;
  LogEntryToDictionary(log_entry, &entry_dict);
  std::string json;
  JSONStringValueSerializer serializer(&json);
  if (!serializer.Serialize(entry_dict)) {
    LOG(ERROR) << "Failed to serialize to JSON";
  }
  return json;
}

void SendLogEntryToVLogOrErrorLog(
    const DeviceEventLogImpl::LogEntry& log_entry) {
  if (log_entry.log_level != LOG_LEVEL_ERROR && !VLOG_IS_ON(1))
    return;
  const bool show_time = true;
  const bool show_file = true;
  const bool show_type = true;
  const bool show_level = log_entry.log_level != LOG_LEVEL_ERROR;
  std::string output =
      LogEntryToString(log_entry, show_time, show_file, show_type, show_level);
  if (log_entry.log_level == LOG_LEVEL_ERROR)
    LOG(ERROR) << output;
  else
    VLOG(1) << output;
}

bool LogEntryMatches(const DeviceEventLogImpl::LogEntry& first,
                     const DeviceEventLogImpl::LogEntry& second) {
  return first.file == second.file && first.file_line == second.file_line &&
         first.log_level == second.log_level &&
         first.log_type == second.log_type && first.event == second.event;
}

bool LogEntryMatchesTypes(const DeviceEventLogImpl::LogEntry& entry,
                          const std::set<LogType>& include_types,
                          const std::set<LogType>& exclude_types) {
  if (include_types.empty() && exclude_types.empty())
    return true;
  if (!include_types.empty() && include_types.count(entry.log_type))
    return true;
  if (!exclude_types.empty() && !exclude_types.count(entry.log_type))
    return true;
  return false;
}

void GetFormat(const std::string& format_string,
               bool* show_time,
               bool* show_file,
               bool* show_type,
               bool* show_level,
               bool* format_json) {
  base::StringTokenizer tokens(format_string, ",");
  *show_time = false;
  *show_file = false;
  *show_type = false;
  *show_level = false;
  *format_json = false;
  while (tokens.GetNext()) {
    std::string tok(tokens.token());
    if (tok == "time")
      *show_time = true;
    if (tok == "file")
      *show_file = true;
    if (tok == "type")
      *show_type = true;
    if (tok == "level")
      *show_level = true;
    if (tok == "json")
      *format_json = true;
  }
}

void GetLogTypes(const std::string& types,
                 std::set<LogType>* include_types,
                 std::set<LogType>* exclude_types) {
  base::StringTokenizer tokens(types, ",");
  while (tokens.GetNext()) {
    std::string tok(tokens.token());
    if (tok.substr(0, 4) == "non-") {
      LogType type = GetLogTypeFromString(tok.substr(4));
      if (type != LOG_TYPE_UNKNOWN)
        exclude_types->insert(type);
    } else {
      LogType type = GetLogTypeFromString(tok);
      if (type != LOG_TYPE_UNKNOWN)
        include_types->insert(type);
    }
  }
}

// Update count and time for identical events to avoid log spam.
void IncreaseLogEntryCount(const DeviceEventLogImpl::LogEntry& new_entry,
                           DeviceEventLogImpl::LogEntry* cur_entry) {
  ++cur_entry->count;
  cur_entry->log_level = std::min(cur_entry->log_level, new_entry.log_level);
  cur_entry->time = base::Time::Now();
}

}  // namespace

// static
void DeviceEventLogImpl::SendToVLogOrErrorLog(const char* file,
                                              int file_line,
                                              LogType log_type,
                                              LogLevel log_level,
                                              const std::string& event) {
  LogEntry entry(file, file_line, log_type, log_level, event);
  SendLogEntryToVLogOrErrorLog(entry);
}

DeviceEventLogImpl::DeviceEventLogImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    size_t max_entries)
    : task_runner_(task_runner), max_entries_(max_entries) {
  DCHECK(task_runner_);
}

DeviceEventLogImpl::~DeviceEventLogImpl() {}

void DeviceEventLogImpl::AddEntry(const char* file,
                                  int file_line,
                                  LogType log_type,
                                  LogLevel log_level,
                                  const std::string& event) {
  LogEntry entry(file, file_line, log_type, log_level, event);
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeviceEventLogImpl::AddLogEntry,
                                  weak_ptr_factory_.GetWeakPtr(), entry));
    return;
  }
  AddLogEntry(entry);
}

void DeviceEventLogImpl::AddEntryWithTimestampForTesting(
    const char* file,
    int file_line,
    LogType log_type,
    LogLevel log_level,
    const std::string& event,
    base::Time time) {
  LogEntry entry(file, file_line, log_type, log_level, event, time);
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DeviceEventLogImpl::AddLogEntry,
                                  weak_ptr_factory_.GetWeakPtr(), entry));
    return;
  }
  AddLogEntry(entry);
}

void DeviceEventLogImpl::AddLogEntry(const LogEntry& entry) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!entries_.empty()) {
    LogEntry& last = entries_.back();
    if (LogEntryMatches(last, entry)) {
      IncreaseLogEntryCount(entry, &last);
      return;
    }
  }
  if (entries_.size() >= max_entries_)
    RemoveEntry();
  entries_.push_back(entry);
  SendLogEntryToVLogOrErrorLog(entry);
}

void DeviceEventLogImpl::RemoveEntry() {
  const size_t max_error_entries = max_entries_ / 2;
  DCHECK(max_error_entries < entries_.size());
  // Remove the first (oldest) non-error entry, or the oldest entry if more
  // than half the entries are errors.
  size_t error_count = 0;
  for (auto iter = entries_.begin(); iter != entries_.end(); ++iter) {
    if (iter->log_level != LOG_LEVEL_ERROR) {
      entries_.erase(iter);
      return;
    }
    if (++error_count > max_error_entries)
      break;
  }
  // Too many error entries, remove the oldest entry.
  entries_.pop_front();
}

std::string DeviceEventLogImpl::GetAsString(StringOrder order,
                                            const std::string& format,
                                            const std::string& types,
                                            LogLevel max_level,
                                            size_t max_events) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (entries_.empty())
    return "No Log Entries.";

  bool show_time, show_file, show_type, show_level, format_json;
  GetFormat(format, &show_time, &show_file, &show_type, &show_level,
            &format_json);

  std::set<LogType> include_types, exclude_types;
  GetLogTypes(types, &include_types, &exclude_types);

  std::string result;
  base::ListValue log_entries;
  if (order == OLDEST_FIRST) {
    size_t offset = 0;
    if (max_events > 0 && max_events < entries_.size()) {
      // Iterate backwards through the list skipping uninteresting entries to
      // determine the first entry to include.
      size_t shown_events = 0;
      size_t num_entries = 0;
      for (const LogEntry& entry : base::Reversed(entries_)) {
        ++num_entries;
        if (!LogEntryMatchesTypes(entry, include_types, exclude_types))
          continue;
        if (entry.log_level > max_level)
          continue;
        if (++shown_events >= max_events)
          break;
      }
      offset = entries_.size() - num_entries;
    }
    for (const LogEntry& entry : entries_) {
      if (offset > 0) {
        --offset;
        continue;
      }
      if (!LogEntryMatchesTypes(entry, include_types, exclude_types))
        continue;
      if (entry.log_level > max_level)
        continue;
      if (format_json) {
        log_entries.AppendString(LogEntryAsJSON(entry));
      } else {
        result += LogEntryToString(entry, show_time, show_file, show_type,
                                   show_level);
        result += "\n";
      }
    }
  } else {
    size_t nlines = 0;
    // Iterate backwards through the list to show the most recent entries first.
    for (const LogEntry& entry : base::Reversed(entries_)) {
      if (!LogEntryMatchesTypes(entry, include_types, exclude_types))
        continue;
      if (entry.log_level > max_level)
        continue;
      if (format_json) {
        log_entries.AppendString(LogEntryAsJSON(entry));
      } else {
        result += LogEntryToString(entry, show_time, show_file, show_type,
                                   show_level);
        result += "\n";
      }
      if (max_events > 0 && ++nlines >= max_events)
        break;
    }
  }
  if (format_json) {
    JSONStringValueSerializer serializer(&result);
    serializer.Serialize(log_entries);
  }

  return result;
}

void DeviceEventLogImpl::ClearAll() {
  entries_.clear();
}

void DeviceEventLogImpl::Clear(const base::Time& begin, const base::Time& end) {
  auto begin_it = std::find_if(
      entries_.begin(), entries_.end(),
      [begin](const LogEntry& entry) { return entry.time >= begin; });
  auto end_rev_it =
      std::find_if(entries_.rbegin(), entries_.rend(),
                   [end](const LogEntry& entry) { return entry.time <= end; });

  entries_.erase(begin_it, end_rev_it.base());
}

DeviceEventLogImpl::LogEntry::LogEntry(const char* filedesc,
                                       int file_line,
                                       LogType log_type,
                                       LogLevel log_level,
                                       const std::string& event)
    : file_line(file_line),
      log_type(log_type),
      log_level(log_level),
      event(event),
      time(base::Time::Now()),
      count(1) {
  if (filedesc) {
    file = filedesc;
    size_t last_slash_pos = file.find_last_of("\\/");
    if (last_slash_pos != std::string::npos) {
      file.erase(0, last_slash_pos + 1);
    }
  }
}

DeviceEventLogImpl::LogEntry::LogEntry(const char* filedesc,
                                       int file_line,
                                       LogType log_type,
                                       LogLevel log_level,
                                       const std::string& event,
                                       base::Time time_for_testing)
    : LogEntry(filedesc, file_line, log_type, log_level, event) {
  time = time_for_testing;
}

DeviceEventLogImpl::LogEntry::LogEntry(const LogEntry& other) = default;

}  // namespace device_event_log
