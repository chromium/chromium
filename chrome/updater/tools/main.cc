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
#include "chrome/updater/tools/certificate_tag.h"

namespace updater {
namespace tools {

// If set, this flag contains a string and a superfluous certificate tag with
// that value will be set and the binary rewritten. If the string begins
// with '0x' then it will be interpreted as hex.
constexpr char kSetSuperfluousCertTagSwitch[] = "set-superfluous-cert-tag";

// A superfluous certificate tag will be padded with zeros to at least this
// number of bytes.
constexpr char kPaddedLength[] = "padded-length";

// If set, this flag causes the current tag, if any, to be written to stdout.
constexpr char kGetSuperfluousCertTagSwitch[] = "get-superfluous-cert-tag";

// If set, the updated binary is written to this file. Otherwise the binary is
// updated in place.
constexpr char kOutFilenameSwitch[] = "out";

struct CommandLineArguments {
  // Whether to print the current tag.
  bool get_superfluous_cert_tag = false;

  // Sets the certificate from bytes.
  std::string set_superfluous_cert_tag;

  // Contains the minimum length of the padding sequence of zeros at the end
  // of the tag.
  int padded_length = 0;

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

  args.get_superfluous_cert_tag =
      cmdline->HasSwitch(kGetSuperfluousCertTagSwitch);
  cmdline->RemoveSwitch(kGetSuperfluousCertTagSwitch);

  args.set_superfluous_cert_tag =
      cmdline->GetSwitchValueASCII(kSetSuperfluousCertTagSwitch);
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

  absl::optional<tools::Binary> bin = tools::Binary::Parse(contents);
  if (!bin) {
    std::cerr << "Failed to parse tag binary." << std::endl;
    std::exit(1);
  }

  if (args.get_superfluous_cert_tag) {
    absl::optional<base::span<const uint8_t>> tag = bin->tag();
    if (!tag) {
      std::cerr << "No tag in binary." << std::endl;
      std::exit(1);
    }

    std::cout << base::HexEncode(*tag) << std::endl;
  }

  if (!args.set_superfluous_cert_tag.empty()) {
    constexpr char kPrefix[] = "0x";
    std::vector<uint8_t> tag_contents;
    if (base::StartsWith(args.set_superfluous_cert_tag, kPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      const auto hex_chars = base::MakeStringPiece(
          std::begin(args.set_superfluous_cert_tag) + std::size(kPrefix) - 1,
          std::end(args.set_superfluous_cert_tag));
      if (!base::HexStringToBytes(hex_chars, &tag_contents)) {
        std::cerr << "Failed to parse tag contents from command line."
                  << std::endl;
        std::exit(1);
      }
    } else {
      tag_contents.assign(args.set_superfluous_cert_tag.begin(),
                          args.set_superfluous_cert_tag.end());
    }
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
