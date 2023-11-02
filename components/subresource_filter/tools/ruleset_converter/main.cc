// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/subresource_filter/tools/ruleset_converter/ruleset_converter.h"

namespace {

const char kSwitchInputFiles[] = "input_files";
const char kSwitchOutputFile[] = "output_file";

const char kSwitchOutputFileUrl[] = "output_file_url";
const char kSwitchOutputFileCss[] = "output_file_css";

const char kSwitchInputFormat[] = "input_format";
const char kSwitchOutputFormat[] = "output_format";

const char kSwitchChromeVersion[] = "chrome_version";

const char kHelpMsg[] = R"(
  ruleset_converter [--input_format=<format>] --output_format=<format>
  --input_files=<path1>[:<path2>...]
  (--output_file=<path> | [--output_file_url=<path>] [--output_file_css=<path>)
  [--chrome_version=<version>]

  ruleset_converter is a utility for converting subresource_filter rulesets
  across multiple formats:

  * --input_files: Comma-separated list of input files with rules. The files
     are processed in the order of declaration

  * --output_file: The file to output the rules. Either this option or at least
    one of the --output_file_url|--output_file_css should be specified.

  * --output_file_url: The file to output URL rules. If equal to
    --output_file_css, the results are merged.

  *  --output_file_css: The file to output CSS rules. See --output_file and
     --output_file_url for details.

  * --input_format: The format of the input file(s). One of
    {filter-list, proto, unindexed-ruleset}

  * --output_format: The format of the output file(s). See --input_format for
    available formats.

  * --chrome_version: The earliest version of Chrome that the produced ruleset
    needs to be compatible with. Currently one of 54, 59, or 0
    (not Chrome-specific). Defaults to the maximum (i.e. 59).
)";

void PrintHelp() {
  printf("%s\n\n", kHelpMsg);
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  if (command_line.GetArgs().size() != 0U) {
    PrintHelp();
    return 1;
  }

  subresource_filter::RulesetConverter converter;
  if (!converter.SetInputFiles(
          command_line.GetSwitchValueNative(kSwitchInputFiles))) {
    PrintHelp();
    return 1;
  }

  if (command_line.HasSwitch(kSwitchOutputFile) &&
      !converter.SetOutputFile(
          command_line.GetSwitchValuePath(kSwitchOutputFile))) {
    PrintHelp();
    return 1;
  }
  if (command_line.HasSwitch(kSwitchOutputFileUrl) &&
      !converter.SetOutputFileUrl(
          command_line.GetSwitchValuePath(kSwitchOutputFileUrl))) {
    PrintHelp();
    return 1;
  }
  if (command_line.HasSwitch(kSwitchOutputFileCss) &&
      !converter.SetOutputFileCss(
          command_line.GetSwitchValuePath(kSwitchOutputFileCss))) {
    PrintHelp();
    return 1;
  }

  if (command_line.HasSwitch(kSwitchChromeVersion) &&
      !converter.SetChromeVersion(
          command_line.GetSwitchValueASCII(kSwitchChromeVersion))) {
    PrintHelp();
    return 1;
  }

  if (command_line.HasSwitch(kSwitchInputFormat) &&
      !converter.SetInputFormat(
          command_line.GetSwitchValueASCII(kSwitchInputFormat))) {
    PrintHelp();
    return 1;
  }
  if (command_line.HasSwitch(kSwitchOutputFormat) &&
      !converter.SetOutputFormat(
          command_line.GetSwitchValueASCII(kSwitchOutputFormat))) {
    PrintHelp();
    return 1;
  }

  if (!converter.Convert())
    return 1;
  return 0;
}
