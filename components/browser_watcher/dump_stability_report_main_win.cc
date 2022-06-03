// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A utility for printing the contents of a postmortem stability minidump.

#include <windows.h>  // NOLINT

#include <dbghelp.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/browser_watcher/activity_report.pb.h"

namespace {

const char kUsage[] =
    "Usage: %ls --minidump=<minidump file>\n"
    "\n"
    "  Dumps the contents of a postmortem minidump in a human readable way.\n";

bool ParseCommandLine(const base::CommandLine* cmd,
                      base::FilePath* minidump_path) {
  *minidump_path = cmd->GetSwitchValuePath("minidump");
  if (minidump_path->empty()) {
    LOG(ERROR) << "Missing minidump file.\n";
    LOG(ERROR) << base::StringPrintf(kUsage, cmd->GetProgram().value().c_str());
    return false;
  }
  return true;
}

void Indent(FILE* out, int indent_level) {
  DCHECK(out);
  for (int i = 0; i < indent_level; ++i)
    fprintf(out, "  ");
}

void PrintUserData(
    FILE* out,
    int indent_level,
    const google::protobuf::Map<std::string, browser_watcher::TypedValue>&
        user_data) {
  DCHECK(out);
  Indent(out, indent_level);
  fprintf(out, "User data (%zu)\n", user_data.size());
  for (const auto& kv : user_data) {
    Indent(out, indent_level + 1);
    fprintf(out, "%s : ", kv.first.c_str());
    const browser_watcher::TypedValue& value = kv.second;
    switch (kv.second.value_case()) {
      case browser_watcher::TypedValue::kBytesValue: {
        const std::string& bytes_value = value.bytes_value();
        for (size_t i = 0; i < bytes_value.size(); ++i)
          fprintf(out, "%02X ", bytes_value.at(i));
        fprintf(out, "\n");
        break;
      }
      case browser_watcher::TypedValue::kBytesReference:
        fprintf(out, "bytes reference (address: %llX, size: %llX)\n",
                value.bytes_reference().address(),
                value.bytes_reference().size());
        break;
      case browser_watcher::TypedValue::kStringValue:
        fprintf(out, "\"%s\"\n", value.string_value().c_str());
        break;
      case browser_watcher::TypedValue::kStringReference:
        fprintf(out, "string reference (address: %llX, size: %llX)\n",
                value.string_reference().address(),
                value.string_reference().size());
        break;
      case browser_watcher::TypedValue::kCharValue:
        fprintf(out, "'%s'\n", value.char_value().c_str());
        break;
      case browser_watcher::TypedValue::kBoolValue:
        fprintf(out, "%s\n", value.bool_value() ? "true" : "false");
        break;
      case browser_watcher::TypedValue::kSignedValue:
        fprintf(out, "%lld\n", value.signed_value());
        break;
      case browser_watcher::TypedValue::kUnsignedValue:
        fprintf(out, "%llu\n", value.unsigned_value());
        break;
      case browser_watcher::TypedValue::VALUE_NOT_SET:
        fprintf(out, "<not set>\n");
        break;
    }
  }
}

void PrintActivity(FILE* out,
                   int indent_level,
                   const browser_watcher::Activity& activity) {
  DCHECK(out);
  Indent(out, indent_level);
  fprintf(out, "Activity\n");
  Indent(out, indent_level + 1);
  fprintf(out, "type: %d\n", activity.type());
  Indent(out, indent_level + 1);
  fprintf(out, "time: %lld\n", activity.time());
  Indent(out, indent_level + 1);
  fprintf(out, "address: %llX\n", activity.address());
  switch (activity.type()) {
    case browser_watcher::Activity::UNKNOWN:
      break;
    case browser_watcher::Activity::ACT_TASK_RUN:
      Indent(out, indent_level + 1);
      fprintf(out, "origin_address: %llX\n", activity.origin_address());
      fprintf(out, "task_sequence_id: %lld\n", activity.task_sequence_id());
      break;
    case browser_watcher::Activity::ACT_LOCK_ACQUIRE:
      Indent(out, indent_level + 1);
      fprintf(out, "lock_address: %llX\n", activity.lock_address());
      break;
    case browser_watcher::Activity::ACT_EVENT_WAIT:
      Indent(out, indent_level + 1);
      fprintf(out, "event_address: %llX\n", activity.event_address());
      break;
    case browser_watcher::Activity::ACT_THREAD_JOIN:
      Indent(out, indent_level + 1);
      fprintf(out, "thread_id: %lld\n", activity.thread_id());
      break;
    case browser_watcher::Activity::ACT_PROCESS_WAIT:
      Indent(out, indent_level + 1);
      fprintf(out, "process_id: %lld\n", activity.process_id());
      break;
    case browser_watcher::Activity::ACT_GENERIC:
      Indent(out, indent_level + 1);
      fprintf(out, "id: %u, data: %d\n", activity.generic_id(),
              activity.generic_data());
      break;
  }

  PrintUserData(out, indent_level + 1, activity.user_data());
}

void PrintProcessState(FILE* out,
                       const browser_watcher::ProcessState& process) {
  std::string process_type;
  switch (process.process_type()) {
    case browser_watcher::ProcessState::UNKNOWN_PROCESS:
      process_type = "unknown type";
      break;
    case browser_watcher::ProcessState::BROWSER_PROCESS:
      process_type = "browser";
      break;
    case browser_watcher::ProcessState::WATCHER_PROCESS:
      process_type = "watcher";
      break;
    default:
      base::SStringPrintf(&process_type, "process type %d",
                          process.process_type());
      break;
  }

  fprintf(out, "Process %lld (%s, %d threads)\n", process.process_id(),
          process_type.c_str(), process.threads_size());

  if (process.has_memory_state() &&
      process.memory_state().has_windows_memory()) {
    const auto& windows_memory = process.memory_state().windows_memory();
    if (windows_memory.has_process_private_usage()) {
      fprintf(out, "process_private_usage: %u pages\n",
              windows_memory.process_private_usage());
    }
    if (windows_memory.has_process_peak_workingset_size()) {
      fprintf(out, "process_peak_workingset_size: %u pages\n",
              windows_memory.process_peak_workingset_size());
    }
    if (windows_memory.has_process_peak_pagefile_usage()) {
      fprintf(out, "process_peak_pagefile_usage: %u pages\n",
              windows_memory.process_peak_pagefile_usage());
    }
    if (windows_memory.has_process_allocation_attempt()) {
      fprintf(out, "process_allocation_attempt: %u bytes\n",
              windows_memory.process_allocation_attempt());
    }
    if (windows_memory.has_process_handle_count()) {
      fprintf(out, "process_handle_count: %u handles\n",
              windows_memory.process_handle_count());
    }
  }

  for (const browser_watcher::ThreadState& thread : process.threads()) {
    fprintf(out, "Thread %lld (%s) : %d activities\n", thread.thread_id(),
            thread.thread_name().c_str(), thread.activity_count());
    for (const browser_watcher::Activity& activity : thread.activities())
      PrintActivity(out, 1, activity);
  }

  PrintUserData(out, 1, process.data());
}

// TODO(manzagop): flesh out as StabilityReport gets fleshed out.
void PrintReport(FILE* out, const browser_watcher::StabilityReport& report) {
  for (std::string message : report.log_messages())
    fprintf(out, "log message:\n%s\n", message.c_str());

  if (report.has_system_memory_state() &&
      report.system_memory_state().has_windows_memory()) {
    const auto& windows_memory = report.system_memory_state().windows_memory();

    if (windows_memory.has_system_commit_limit()) {
      fprintf(out, "system_commit_limit: %u pages\n",
              windows_memory.system_commit_limit());
    }
    if (windows_memory.has_system_commit_remaining()) {
      fprintf(out, "system_commit_remaining: %u pages\n",
              windows_memory.system_commit_remaining());
    }
    if (windows_memory.has_system_handle_count()) {
      fprintf(out, "system_handle_count: %u handles\n",
              windows_memory.system_handle_count());
    }
  }
  PrintUserData(out, 0, report.global_data());
  for (int i = 0; i < report.process_states_size(); ++i) {
    const browser_watcher::ProcessState process = report.process_states(i);
    PrintProcessState(out, process);
  }
}

bool GetStabilityStreamRvaAndSize(RVA directory_rva,
                                  ULONG32 stream_count,
                                  FILE* file,
                                  RVA* report_rva,
                                  ULONG32* report_size_bytes) {
  std::vector<MINIDUMP_DIRECTORY> directory;
  directory.resize(stream_count);

  CHECK_EQ(0, fseek(file, directory_rva, SEEK_SET));
  CHECK_EQ(stream_count, fread(directory.data(), sizeof(MINIDUMP_DIRECTORY),
                               stream_count, file));

  for (const MINIDUMP_DIRECTORY& entry : directory) {
    constexpr ULONG32 kActivityStream = static_cast<ULONG32>(0x4B6B0002);
    if (entry.StreamType == kActivityStream) {
      *report_rva = entry.Location.Rva;
      *report_size_bytes = entry.Location.DataSize;
      return true;
    }
  }

  return false;
}

int Main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Get the dump.
  base::FilePath minidump_path;
  if (!ParseCommandLine(base::CommandLine::ForCurrentProcess(), &minidump_path))
    return 1;

  // Read the minidump to extract the proto.
  base::ScopedFILE minidump_file;
  minidump_file.reset(base::OpenFile(minidump_path, "rb"));
  CHECK(minidump_file.get());

  // Read the header.
  // TODO(manzagop): leverage Crashpad to do this.
  MINIDUMP_HEADER header = {};
  CHECK_EQ(1U, fread(&header, sizeof(header), 1U, minidump_file.get()));
  CHECK_EQ(static_cast<ULONG32>(MINIDUMP_SIGNATURE), header.Signature);
  fprintf(stdout, "Number of streams: %u\n", header.NumberOfStreams);
  RVA directory_rva = header.StreamDirectoryRva;

  RVA report_rva;
  ULONG32 report_size_bytes;
  CHECK(GetStabilityStreamRvaAndSize(directory_rva, header.NumberOfStreams,
                                     minidump_file.get(), &report_rva,
                                     &report_size_bytes));

  // Read the serialized stability report.
  std::string serialized_report;
  serialized_report.resize(report_size_bytes);
  CHECK_EQ(0, fseek(minidump_file.get(), report_rva, SEEK_SET));
  CHECK_EQ(report_size_bytes, fread(&serialized_report.at(0), 1,
                                    report_size_bytes, minidump_file.get()));

  browser_watcher::StabilityReport report;
  CHECK(report.ParseFromString(serialized_report));

  // Note: we can't use the usual protocol buffer human readable API due to
  // the use of optimize_for = LITE_RUNTIME.
  PrintReport(stdout, report);

  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  return Main(argc, argv);
}
