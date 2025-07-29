// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/command_output_user_stream.h"

#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"
#include "third_party/zlib/zlib.h"

namespace crash_reporter {

namespace {

// Some commands produce a lot of output, but it’s text, so it compresses fairly
// well. Use zlib to compress it to not produce extra-large minidumps.
std::optional<std::string> ZlibCompress(const std::string& uncompressed) {
  if (uncompressed.empty()) {
    return std::nullopt;
  }

  uLongf compressed_size = compressBound(uncompressed.size());
  std::string compressed(compressed_size, '\0');
  int compress_result = compress2(
      reinterpret_cast<unsigned char*>(compressed.data()), &compressed_size,
      reinterpret_cast<const unsigned char*>(uncompressed.data()),
      uncompressed.size(), Z_DEFAULT_COMPRESSION);
  if (compress_result != Z_OK) {
    return std::nullopt;
  }

  compressed.resize(compressed_size);
  return compressed;
}

class CommandOutputUserStreamDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  CommandOutputUserStreamDataSource(
      const std::vector<std::vector<std::string>>& commands)
      : MinidumpUserExtensionStreamDataSource(kStreamType) {
    // This is a simple serialization format that encodes a sequence of
    // (commands), where each command in (commands) is a sequence of (args,
    // exit_status, stdout). args is a sequence created from each command’s
    // argv, and exit_status is an int, or 'x' if the command could not be run.
    // stdout is preceded by a number indicating whether it’s presented
    // uncompressed (0) or zlib-compressed (1). sequences are preceded by the
    // number of contained elements, and strings such as arguments and (possibly
    // compressed) stdout are preceded by their encoded byte length.
    //
    // This format can be decoded by
    // https://chromium-review.googlesource.com/c/6791935.
    data_ = base::StringPrintf("%u ", commands.size());
    for (const std::vector<std::string>& command : commands) {
      base::StringAppendF(&data_, "%u ", command.size());
      for (const std::string& arg : command) {
        base::StringAppendF(&data_, "%u ", arg.size());
        data_ += arg;
      }

      int status;
      std::string uncompressed_stdout;
      if (!base::GetAppOutputWithExitCode(command, &uncompressed_stdout,
                                          &status)) {
        data_ += "x ";
      } else {
        base::StringAppendF(&data_, "%d ", status);
      }

      std::optional<std::string> compressed_stdout =
          ZlibCompress(uncompressed_stdout);
      int stdout_format;
      const std::string* which_stdout;
      if (compressed_stdout &&
          compressed_stdout->size() < uncompressed_stdout.size()) {
        stdout_format = 1;
        which_stdout = &compressed_stdout.value();
      } else {
        stdout_format = 0;
        which_stdout = &uncompressed_stdout;
      }
      base::StringAppendF(&data_, "%d %u ", stdout_format,
                          which_stdout->size());
      data_ += *which_stdout;
    }
  }

  CommandOutputUserStreamDataSource(const CommandOutputUserStreamDataSource&) =
      delete;
  CommandOutputUserStreamDataSource& operator=(
      const CommandOutputUserStreamDataSource&) = delete;

  ~CommandOutputUserStreamDataSource() final = default;

  size_t StreamDataSize() final { return data_.size(); }

  bool ReadStreamData(Delegate* delegate) final {
    return delegate->ExtensionStreamDataSourceRead(data_.data(), data_.size());
  }

 private:
  static constexpr uint32_t kStreamType = 0x4b6b0005;

  std::string data_;
};

}  // namespace

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
CommandOutputUserStream::ProduceStreamData(
    crashpad::ProcessSnapshot* process_snapshot) {
  DCHECK(process_snapshot);

  // Look for the magic crash key that says that the CommandOutputUserStream
  // stream should be created.
  bool found = false;
  for (const crashpad::ModuleSnapshot* module_snapshot :
       process_snapshot->Modules()) {
    for (const crashpad::AnnotationSnapshot& annotation_snapshot :
         module_snapshot->AnnotationObjects()) {
      if (annotation_snapshot.name == "net-crbug_40064248" &&
          annotation_snapshot.type ==
              static_cast<std::underlying_type_t<crashpad::Annotation::Type>>(
                  crashpad::Annotation::Type::kString)) {
        std::string value(
            reinterpret_cast<const char*>(annotation_snapshot.value.data()),
            annotation_snapshot.value.size());
        if (value == "1") {
          found = true;
          break;
        }
      }
    }
    if (found) {
      break;
    }
  }

  if (!found) {
    // The magic crash key wasn’t present. Don’t include a
    // CommandOutputUserStream in the minidump.
    return nullptr;
  }

  std::vector<std::vector<std::string>> commands{
      // Network interface configuration.
      {"/sbin/ifconfig", "-aLmrv"},

      // Per-interface statistics.
      {"/usr/sbin/netstat", "-abdilnvW"},

      // Routing table.
      {"/usr/sbin/netstat", "-alllnr"},

      // BPF statistics (not supported on all OS versions).
      {"/usr/sbin/netstat", "-Bn"},

      // Network stack memory management.
      {"/usr/sbin/netstat", "-mmn"},

      // System extensions (not to be confused with kernel extensions).
      {"/usr/bin/systemextensionsctl", "list"},

      // Kernel extensions.
      {"/usr/bin/kmutil", "showloaded"},

      // Files opened by the crashing process.
      {"/usr/sbin/lsof", "-lnPR", "+f", "cg", "-g", "+L", "-T", "fqs", "-p",
       base::StringPrintf("%d", process_snapshot->ProcessID())},
  };

  static int count;
  if (count++ == 0) {
    // The system profile.
    //
    // This can take a long time (~9s on a M1 MacBookPro18,2 running macOS 15.5
    // 24F74), so only do it once per chrome_crashpad_handler process.
    //
    // The use of the static is thread-safe because this runs exclusively on the
    // single handler thread in chrome_crashpad_handler.
    commands.push_back(
        {"/usr/sbin/system_profiler", "-xml", "-detailLevel", "full"});
  }

  return std::make_unique<CommandOutputUserStreamDataSource>(commands);
}

}  // namespace crash_reporter
