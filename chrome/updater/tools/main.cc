// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/updater/certificate_tag.h"
#include "chrome/updater/tag.h"

namespace updater {
namespace tools {

// If set, a superfluous certificate will be written into the binary. A tag
// string can be optionally passed as the value for this argument, in which case
// the tag string will be validated and set with the appropriate magic signature
// within the certificate.
constexpr char kSetSuperfluousCertTagSwitch[] = "set-superfluous-cert-tag";

// A superfluous certificate tag will be padded with zeros to at least this
// number of bytes. The default is 8206 bytes if this parameter is not set.
constexpr char kPaddedLength[] = "padded-length";

// If set, this flag causes the current tag string, if any, to be written to
// stdout.
constexpr char kGetTagStringSwitch[] = "get-tag-string";

// If set, the updated binary is written to this file. Otherwise the binary is
// updated in place.
constexpr char kOutFilenameSwitch[] = "out";

struct CommandLineArguments {
  // Whether to print the current tag string.
  bool get_tag_string = false;

  // Whether to set a superfluous certificate within the binary.
  bool set_superfluous_cert = false;

  // If set, the tag string will be validated and set with the appropriate magic
  // signature within the superfluous certificate.
  std::string tag_string;

  // Contains the minimum length of the padding sequence of zeros at the end
  // of the tag.
  int padded_length = 8206;

  // Specifies the input file (which may be the same as the output file).
  base::FilePath in_filename;

  // Specifies the file name for the output of operations.
  base::FilePath out_filename;
};

void PrintUsageAndExit(const base::CommandLine* cmdline) {
  std::cerr << "Usage: " << cmdline->GetProgram().MaybeAsASCII()
            << " [flags] binary.exe" << std::endl;
  std::exit(255);
}

void HandleError(int error) {
  std::cerr << "Error: " << error << std::endl;
  std::exit(1);
}

CommandLineArguments ParseCommandLineArgs(int argc, char** argv) {
  CommandLineArguments args;
  base::CommandLine::Init(argc, argv);
  auto* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->argv().size() == 1 || cmdline->GetArgs().size() != 1)
    PrintUsageAndExit(cmdline);

  args.in_filename = base::FilePath{cmdline->GetArgs()[0]};

  const base::FilePath out_filename =
      cmdline->GetSwitchValuePath(kOutFilenameSwitch);
  cmdline->RemoveSwitch(kOutFilenameSwitch);
  args.out_filename = out_filename;

  args.get_tag_string = cmdline->HasSwitch(kGetTagStringSwitch);
  cmdline->RemoveSwitch(kGetTagStringSwitch);

  args.set_superfluous_cert = cmdline->HasSwitch(kSetSuperfluousCertTagSwitch);
  args.tag_string = cmdline->GetSwitchValueASCII(kSetSuperfluousCertTagSwitch);
  cmdline->RemoveSwitch(kSetSuperfluousCertTagSwitch);

  if (cmdline->HasSwitch(kPaddedLength)) {
    int padded_length = 0;
    if (!base::StringToInt(cmdline->GetSwitchValueASCII(kPaddedLength),
                           &padded_length) ||
        padded_length < 0) {
      PrintUsageAndExit(cmdline);
    }
    args.padded_length = padded_length;
    cmdline->RemoveSwitch(kPaddedLength);
  }

  const auto unknown_switches = cmdline->GetSwitches();
  if (!unknown_switches.empty()) {
    std::cerr << "Unknown command line switch: "
              << unknown_switches.begin()->first << std::endl;
    PrintUsageAndExit(cmdline);
  }

  return args;
}

int CertificateTagMain(int argc, char** argv) {
  const auto args = ParseCommandLineArgs(argc, argv);

  const base::FilePath in_filename = args.in_filename;
  const base::FilePath out_filename =
      args.out_filename.empty() ? args.in_filename : args.out_filename;

  int64_t in_filename_size = 0;
  if (!base::GetFileSize(in_filename, &in_filename_size))
    HandleError(logging::GetLastSystemErrorCode());

  std::vector<uint8_t> contents(in_filename_size);
  if (base::ReadFile(in_filename, reinterpret_cast<char*>(&contents.front()),
                     contents.size()) == -1) {
    HandleError(logging::GetLastSystemErrorCode());
  }

  absl::optional<tagging::Binary> bin = tagging::Binary::Parse(contents);
  if (!bin) {
    std::cerr << "Failed to parse tag binary." << std::endl;
    std::exit(1);
  }

  if (args.get_tag_string) {
    absl::optional<base::span<const uint8_t>> tag = bin->tag();
    if (!tag) {
      std::cerr << "No tag in binary." << std::endl;
      std::exit(1);
    }

    const std::vector<const uint8_t> tag_data = {tag->begin(), tag->end()};
    const std::string tag_string =
        tagging::ReadTagUtf8(tag_data.begin(), tag_data.end());
    if (tag_string.empty()) {
      std::cerr << "No tag string embedded in the binary." << std::endl;
      std::exit(1);
    }

    std::cout << tag_string << std::endl;
  }

  if (args.set_superfluous_cert) {
    // Validate the tag string, if any.
    if (!args.tag_string.empty()) {
      tagging::TagArgs tag_args;
      const tagging::ErrorCode error =
          tagging::Parse(args.tag_string, {}, &tag_args);
      if (error != tagging::ErrorCode::kSuccess) {
        std::cerr << "Tag string is invalid: " << args.tag_string << std::endl;
        std::exit(1);
      }
    }

    std::vector<uint8_t> tag_contents =
        tagging::GetTagFromTagString(args.tag_string);

    if (args.padded_length > 0) {
      size_t new_size = 0;
      if (base::CheckAdd(tag_contents.size(), args.padded_length)
              .AssignIfValid(&new_size)) {
        tag_contents.resize(new_size);
      } else {
        std::cerr << "Failed to pad the tag contents." << std::endl;
        std::exit(1);
      }
    }

    auto new_contents = bin->SetTag(tag_contents);
    if (!new_contents) {
      std::cerr << "Error while setting superfluous certificate tag.";
      std::exit(1);
    }
    if (!base::WriteFile(out_filename, *new_contents)) {
      std::cerr << "Error while writing updated file "
                << logging::GetLastSystemErrorCode();
      std::exit(1);
    }
  }

  return EXIT_SUCCESS;
}

}  // namespace tools
}  // namespace updater

int main(int argc, char** argv) {
  return updater::tools::CertificateTagMain(argc, argv);
}
