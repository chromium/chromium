// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "components/gwp_asan/crash_handler/crash_handler.h"
#include "third_party/crashpad/crashpad/minidump/minidump_extensions.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/util/file/file_reader.h"

namespace gwp_asan {

std::string ErrorTypeToString(Crash::ErrorType error_type) {
  switch (error_type) {
    case Crash::USE_AFTER_FREE:
      return "USE_AFTER_FREE";
    case Crash::BUFFER_UNDERFLOW:
      return "BUFFER_UNDERFLOW";
    case Crash::BUFFER_OVERFLOW:
      return "BUFFER_OVERFLOW";
    case Crash::DOUBLE_FREE:
      return "DOUBLE_FREE";
    case Crash::UNKNOWN:
      return "UNKNOWN";
    case Crash::FREE_INVALID_ADDRESS:
      return "FREE_INVALID_ADDRESS";
    default:
      return "UNKNOWN";
  }
}

std::string AllocatorToString(Crash::Allocator allocator) {
  switch (allocator) {
    case Crash::MALLOC:
      return "MALLOC";
    case Crash::PARTITIONALLOC:
      return "PARTITIONALLOC";
    default:
      return "UNKNOWN";
  }
}

std::string ModeToString(Crash::Mode mode) {
  switch (mode) {
    case Crash::CLASSIC:
      return "CLASSIC";
    case Crash::LIGHTWEIGHT_DETECTOR_BRP:
      return "LIGHTWEIGHT_DETECTOR_BRP";
    case Crash::LIGHTWEIGHT_DETECTOR_RANDOM:
      return "LIGHTWEIGHT_DETECTOR_RANDOM";
    case Crash::EXTREME_LIGHTWEIGHT_DETECTOR:
      return "EXTREME_LIGHTWEIGHT_DETECTOR";
    default:
      return "UNKNOWN";
  }
}

std::string AllocationInfoToString(const Crash::AllocationInfo& info) {
  std::stringstream ss;
  ss << "AllocationInfo {" << std::endl;
  ss << "    stack_trace: [" << std::endl;
  for (int i = 0; i < info.stack_trace_size(); ++i) {
    ss << "      " << info.stack_trace(i)
       << (i < info.stack_trace_size() - 1 ? ", " : "") << std::endl;
  }
  ss << "    ]" << std::endl;
  ss << "    thread_id: " << info.thread_id() << std::endl;
  ss << "  }";
  return ss.str();
}

std::string CrashToString(const Crash& crash) {
  std::stringstream ss;
  ss << "Crash {" << std::endl;
  if (crash.has_error_type()) {
    ss << "  error_type: " << ErrorTypeToString(crash.error_type())
       << std::endl;
  }
  if (crash.has_allocation_address()) {
    ss << "  allocation_address: " << crash.allocation_address() << std::endl;
  }
  if (crash.has_allocation_size()) {
    ss << "  allocation_size: " << crash.allocation_size() << std::endl;
  }
  if (crash.has_allocation()) {
    ss << "  allocation: " << AllocationInfoToString(crash.allocation())
       << std::endl;
  }
  if (crash.has_deallocation()) {
    ss << "  deallocation: " << AllocationInfoToString(crash.deallocation())
       << std::endl;
  }
  if (crash.has_region_start()) {
    ss << "  region_start: " << crash.region_start() << std::endl;
  }
  if (crash.has_region_size()) {
    ss << "  region_size: " << crash.region_size() << std::endl;
  }
  if (crash.has_free_invalid_address()) {
    ss << "  free_invalid_address: " << crash.free_invalid_address()
       << std::endl;
  }
  if (crash.has_missing_metadata()) {
    ss << "  missing_metadata: "
       << (crash.missing_metadata() ? "true" : "false") << std::endl;
  }
  if (crash.has_internal_error()) {
    ss << "  internal_error: " << crash.internal_error() << std::endl;
  }
  if (crash.has_allocator()) {
    ss << "  allocator: " << AllocatorToString(crash.allocator()) << std::endl;
  }
  if (crash.has_mode()) {
    ss << "  mode: " << ModeToString(crash.mode()) << std::endl;
  }
  ss << "}";
  return ss.str();
}

}  // namespace gwp_asan

int main(int argc, const char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() != 1) {
    std::cerr << "Usage: dump_gwp_asan <minidump_path>" << std::endl;
    return 1;
  }

  base::FilePath path(args[0]);

  crashpad::FileReader minidump_file_reader;
  if (!minidump_file_reader.Open(path)) {
    std::cerr << "Can't open the minidump." << std::endl;
    return 1;
  }

  crashpad::ProcessSnapshotMinidump minidump_process_snapshot;
  if (!minidump_process_snapshot.Initialize(&minidump_file_reader)) {
    std::cerr << "Can't initialize the process snapshot." << std::endl;
    return 1;
  }

  gwp_asan::Crash proto;
  bool found = false;
  auto custom_streams = minidump_process_snapshot.CustomMinidumpStreams();
  for (auto* stream : custom_streams) {
    if (stream->stream_type() ==
        static_cast<crashpad::MinidumpStreamType>(
            gwp_asan::internal::kGwpAsanMinidumpStreamType)) {
      if (!proto.ParseFromArray(stream->data().data(), stream->data().size())) {
        std::cerr << "Couldn't parse the GWP-ASan stream." << std::endl;
        return 1;
      }
      found = true;
      break;
    }
  }

  if (!found) {
    std::cerr << "Couldn't find the GWP-ASan stream." << std::endl;
  }

  std::cout << gwp_asan::CrashToString(proto) << std::endl;

  return 0;
}
