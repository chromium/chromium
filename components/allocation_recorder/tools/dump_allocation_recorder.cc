// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This implements a small tool to dump the recorded data of the allocation
// recorder from a Crashpad dump to cout.
//
// Invocation: dump_allocation_recorder_data <dump_file>
// When calling the tool without a dump or the given dump contains invalid data,
// an error message will be printed.
//
// Calling the tool with a valid dump file which contains data included from the
// allocation recorder will result in all recorded data being printed to the
// screen.
//
// Example output:
// ===================================================================
// Operation-Id: 0
// Type: FREE
// Address: 0x78802fd2980
// Size: not set
// Frames:
// # 0: libfoo.so + 0x2e404 (0x795e4ef2e404)
// [ ... ]
// #15: libbar.so + 0xdaa00 (0x795de0fdaa00)
// ===================================================================
// Operation-Id: 1
// Type: FREE
// Address: 0x78802fd2b00
// Size: not set
// Frames:
// # 0: libfoo.so + 0x2e404 (0x795e4ef2e404)
// [ ... ]
// #14: libbaz.so + 0x55eb9 (0x795e0f155eb9)
// ===================================================================
// [ many more entries here ]
// ===================================================================
//
// The call stack can than be symbolized using tools like stack.py or
// llvm-symbolize.

#include <iomanip>
#include <iostream>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "components/allocation_recorder/crash_handler/memory_operation_report.pb.h"
#include "components/allocation_recorder/internal/internal.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/util/file/file_reader.h"

namespace allocation_recorder {
// Implement operator << for various types of the allocation recorder payload.
// The functions must live in the same namespace as the type to print.
// Therefore, they can't be moved into anonymous namespace.

std::ostream& operator<<(std::ostream& out_stream,
                         const OperationType& operation_type) {
  switch (operation_type) {
    case OperationType::ALLOCATION:
      out_stream << "ALLOCATION";
      break;
    case OperationType::FREE:
      out_stream << "FREE";
      break;
    case OperationType::NONE:
      out_stream << "NONE";
      break;
    default:
      // Some internal values are added by protobuf to the enum. Those values
      // shouldn't play a role, but the compiler complains if we do not handle
      // them.
      NOTREACHED();
  }
  return out_stream;
}

struct StackFrameAndMinidump {
  const StackFrame& frame;
  const crashpad::ProcessSnapshotMinidump& minidump;
};

std::ostream& operator<<(std::ostream& out_stream,
                         const StackFrameAndMinidump& frame_and_minidump) {
  const StackFrame& frame = frame_and_minidump.frame;
  const crashpad::ProcessSnapshotMinidump& minidump =
      frame_and_minidump.minidump;

  uint64_t relative_addr = 0;
  std::string module_name = "<unknown-module>";
  for (const crashpad::ModuleSnapshot* module : minidump.Modules()) {
    if (frame.address() >= module->Address() &&
        frame.address() < module->Address() + module->Size()) {
      relative_addr = frame.address() - module->Address();
      module_name = module->Name();
      break;
    }
  }

  out_stream << module_name << " + 0x" << std::hex << relative_addr << " (0x"
             << frame.address() << ")";
  return out_stream;
}

struct StackTraceAndMinidump {
  const StackTrace& stack_trace;
  const crashpad::ProcessSnapshotMinidump& minidump;
};

std::ostream& operator<<(
    std::ostream& out_stream,
    const StackTraceAndMinidump& stack_trace_and_minidump) {
  const StackTrace& stack_trace = stack_trace_and_minidump.stack_trace;
  const crashpad::ProcessSnapshotMinidump& minidump =
      stack_trace_and_minidump.minidump;

  if (stack_trace.frames_size() > 0) {
    const auto print_frame = [&](int frame_index, bool print_separator) {
      out_stream << '#' << std::setfill('0') << std::setw(2) << std::dec
                 << frame_index << ": "
                 << StackFrameAndMinidump{stack_trace.frames(frame_index),
                                          minidump};
      if (print_separator) {
        out_stream << '\n';
      }
    };

    const int last_frame_index = stack_trace.frames_size() - 1;

    for (int frame_index = 0; frame_index < last_frame_index; ++frame_index) {
      print_frame(frame_index, true);
    }

    print_frame(last_frame_index, false);
  }

  return out_stream;
}

struct MemoryOperationAndProcessMinidump {
  const MemoryOperation& operation;
  const crashpad::ProcessSnapshotMinidump& minidump;
};

std::ostream& operator<<(
    std::ostream& out_stream,
    const MemoryOperationAndProcessMinidump& operation_and_minidump) {
  const MemoryOperation& operation = operation_and_minidump.operation;
  const crashpad::ProcessSnapshotMinidump& minidump =
      operation_and_minidump.minidump;

  out_stream << "Type: " << operation.operation_type() << '\n';
  out_stream << "Address: " << std::hex << "0x" << operation.address() << '\n';

  out_stream << "Size: ";
  if (operation.has_size()) {
    out_stream << std::dec << operation.size();
  } else {
    out_stream << "not set";
  }
  out_stream << '\n';

  out_stream << "Frames:\n";
  if (operation.has_stack_trace()) {
    out_stream << StackTraceAndMinidump{operation.stack_trace(), minidump};
  } else {
    out_stream << "not set";
  }
  out_stream << '\n';

  return out_stream;
}

namespace {

const char* const entry_separator =
    "===================================================================\n";

base::expected<allocation_recorder::Payload, std::string>
FindAndDeserializeAllocationRecorderStream(
    const crashpad::ProcessSnapshotMinidump& minidump) {
  const std::vector<const crashpad::MinidumpStream*>& custom_streams =
      minidump.CustomMinidumpStreams();

  const auto it_user_stream = std::find_if(
      custom_streams.begin(), custom_streams.end(),
      [](const crashpad::MinidumpStream* user_stream) {
        return user_stream &&
               user_stream->stream_type() ==
                   static_cast<crashpad::MinidumpStreamType>(
                       allocation_recorder::internal::kStreamDataType);
      });

  if (it_user_stream == custom_streams.end()) {
    return base::unexpected("No allocation recorder stream found.");
  }

  allocation_recorder::Payload report;
  if (!report.ParseFromArray((*it_user_stream)->data().data(),
                             (*it_user_stream)->data().size())) {
    return base::unexpected(
        "The stream doesn't contain a valid allocation recorder payload.");
  }

  return base::ok(report);
}

void Print(const allocation_recorder::MemoryOperationReport& report,
           std::ostream& out_stream,
           const crashpad::ProcessSnapshotMinidump& minidump_process_snapshot) {
  if (report.memory_operations_size() == 0) {
    out_stream << "NO RECORDS\n";
  } else {
    const auto print_memory_operation = [&](int operation_index,
                                            bool print_entry_separator) {
      out_stream << "Operation-Id: " << std::dec << operation_index << '\n'
                 << MemoryOperationAndProcessMinidump{
                        report.memory_operations(operation_index),
                        minidump_process_snapshot};
      if (print_entry_separator) {
        out_stream << entry_separator;
      }
    };

    const int last_memory_operation_index = report.memory_operations_size() - 1;

    for (int memory_operation_index = 0;
         memory_operation_index < last_memory_operation_index;
         ++memory_operation_index) {
      print_memory_operation(memory_operation_index, true);
    }

    print_memory_operation(last_memory_operation_index, false);
  }
}

void Print(const allocation_recorder::ProcessingFailures& failures,
           std::ostream& out_stream) {
  if (failures.messages_size() > 0) {
    const auto& print_failure_message = [&](int message_index,
                                            bool print_entry_separator) {
      out_stream << failures.messages(message_index);
      if (print_entry_separator) {
        out_stream << entry_separator;
      }
    };

    const int last_message_index = failures.messages_size() - 1;

    for (int message_index = 0; message_index < failures.messages_size();
         ++message_index) {
      print_failure_message(message_index, true);
    }
    print_failure_message(last_message_index, false);
  }
}

void PrintPayload(
    const allocation_recorder::Payload& payload,
    const crashpad::ProcessSnapshotMinidump& minidump_process_snapshot) {
  std::ostream& out_stream = std::cout;

  out_stream << entry_separator;

  switch (payload.payload_case()) {
    case allocation_recorder::Payload::PayloadCase::kOperationReport:
      CHECK(payload.has_operation_report());
      Print(payload.operation_report(), out_stream, minidump_process_snapshot);
      break;
    case allocation_recorder::Payload::PayloadCase::kProcessingFailures:
      CHECK(payload.has_processing_failures());
      Print(payload.processing_failures(), out_stream);
      break;
    default:
      // Some internal values are added by protobuf to the enum. Those values
      // shouldn't play a role, but the compiler complains if we do not handle
      // them.
      NOTREACHED();
  }

  out_stream << entry_separator;
}

template <typename... T>
void PrintError(T&... message_parts) {
  ((std::cerr << std::forward<T>(message_parts)), ...) << "\n";
}

}  // namespace
}  // namespace allocation_recorder

int main(int argc, const char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() != 1) {
    allocation_recorder::PrintError("Usage: ", argv[0], " <minidump_path>");
    return 1;
  }

  base::FilePath path(args[0]);

  crashpad::FileReader minidump_file_reader;
  if (!minidump_file_reader.Open(path)) {
    allocation_recorder::PrintError("Can't open the minidump.");
    return 1;
  }

  crashpad::ProcessSnapshotMinidump minidump_process_snapshot;
  if (!minidump_process_snapshot.Initialize(&minidump_file_reader)) {
    allocation_recorder::PrintError("Can't initialize the process snapshot.");
    return 1;
  }

  auto payload_deserialization =
      allocation_recorder::FindAndDeserializeAllocationRecorderStream(
          minidump_process_snapshot);

  if (payload_deserialization.has_value()) {
    allocation_recorder::PrintPayload(payload_deserialization.value(),
                                      minidump_process_snapshot);
  } else {
    allocation_recorder::PrintError(payload_deserialization.error());
  }

  return 0;
}
