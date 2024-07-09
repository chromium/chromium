// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/updater/certificate_tag.h"
#include "chrome/updater/tag.h"

namespace updater {
namespace tools {

// If set:
// * For EXEs, a superfluous certificate will be written into the binary. A tag
//   string can be optionally passed as the value for this argument, in which
//   case the tag string will be validated and set with the appropriate magic
//   signature within the certificate.
// * For MSIs, the tag string is a required argument, and will be validated and
//   set with the appropriate magic signature within the MSI.
constexpr char kSetTagSwitch[] = "set-tag";

// For EXEs, a superfluous certificate tag will be padded with zeros to at least
// this number of bytes. The default is 8206 bytes if this parameter is not set.
// For MSIs, this parameter is ignored.
constexpr char kPaddedLength[] = "padded-length";

// If set, this flag causes the current tag, if any, to be written to stdout.
constexpr char kGetTagSwitch[] = "get-tag";

// If set, the updated binary is written to this file. Otherwise the binary is
// updated in place.
constexpr char kOutFilenameSwitch[] = "out";

struct CommandLineArguments {
  // Whether to print the current tag string.
  bool get_tag_string = false;

  // Whether to set a superfluous certificate within the EXE.
  bool set_superfluous_cert = false;

  // If set, the tag string will be validated and set within the binary.
  std::string tag_string;

  // Contains the minimum length of the padding sequence of zeros at the end
  // of the tag for EXEs.
  int padded_length = 8206;

  // Specifies the input file (which may be the same as the output file).
  base::FilePath in_filename;

  // Specifies the file name for the output of operations.
  base::FilePath out_filename;
};

void PrintUsageAndExit(const base::CommandLine* cmdline) {
  std::cerr << "Usage: " << cmdline->GetProgram().MaybeAsASCII()
            << " [--get-tag|set-tag=TAG] [--padded-length=PADDED_LENGTH]"
            << " [--out=OUT] binary.[exe|msi]" << std::endl;
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
  if (cmdline->argv().size() == 1 || cmdline->GetArgs().size() != 1) {
    PrintUsageAndExit(cmdline);
  }

  args.in_filename = base::FilePath{cmdline->GetArgs()[0]};

  const base::FilePath out_filename =
      cmdline->GetSwitchValuePath(kOutFilenameSwitch);
  cmdline->RemoveSwitch(kOutFilenameSwitch);
  args.out_filename = out_filename;

  args.get_tag_string = cmdline->HasSwitch(kGetTagSwitch);
  cmdline->RemoveSwitch(kGetTagSwitch);

  args.set_superfluous_cert = cmdline->HasSwitch(kSetTagSwitch);
  args.tag_string = cmdline->GetSwitchValueASCII(kSetTagSwitch);
  cmdline->RemoveSwitch(kSetTagSwitch);

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

  const auto unexpected_switches = cmdline->GetSwitches();
  if (!unexpected_switches.empty()) {
    std::cerr << "Unexpected command line switch: "
              << unexpected_switches.begin()->first << std::endl;
    PrintUsageAndExit(cmdline);
  }

  return args;
}

int TagMain(int argc, char** argv) {
  const auto args = ParseCommandLineArgs(argc, argv);
  if (args.get_tag_string) {
    const std::string tag_string =
        tagging::BinaryReadTagString(args.in_filename);
    if (tag_string.empty()) {
      std::cout << "Could not get tag string, see log for details" << std::endl;
      std::exit(1);
    }
    std::cout << tag_string << std::endl;
  }

  if (args.set_superfluous_cert) {
    if (!tagging::BinaryWriteTag(
            args.in_filename, args.tag_string, args.padded_length,
            args.out_filename.empty() ? args.in_filename : args.out_filename)) {
      std::cout << "Could not write tag, see log for details" << std::endl;
      std::exit(1);
    }
  }

  return EXIT_SUCCESS;
}

}  // namespace tools
}  // namespace updater

int main(int argc, char** argv) {
  return updater::tools::TagMain(argc, argv);
}
