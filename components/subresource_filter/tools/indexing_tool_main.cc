// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "components/subresource_filter/tools/indexing_tool.h"

const char kHelpMsg[] = R"(
  subresource_indexing_tool <unindexed_ruleset_file> <output_file>
  [--version_output=<version_output> --content_version=<content_version>]

  subresource_indexing_tool will open the |unindexed_ruleset_file| and output
  an indexed version in |output_file|.

  Optionally, this tool can also output version metadata json, given a content
  version. This metadata matches how versions are stored in Chrome prefs.
)";

const char kSwitchVersionOutput[] = "version_output";
const char kSwitchContentVersion[] = "content_version";

void PrintHelp() {
  printf("%s\n\n", kHelpMsg);
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line.GetArgs();

  if (args.size() < 2U) {
    PrintHelp();
    return 1;
  }

  base::FilePath unindexed_path(args[0]);
  base::FilePath indexed_path(args[1]);

  int checksum = 0;
  if (!subresource_filter::IndexAndWriteRuleset(unindexed_path, indexed_path,
                                                &checksum)) {
    LOG(ERROR) << "There was an error. Be sure that the first argument points "
                  "to a valid unindexed file and that the second argument is "
                  "in an existing directory.";
    return 1;
  }

  CHECK_NE(0, checksum);

  if (!command_line.HasSwitch(kSwitchVersionOutput))
    return 0;

  LOG_IF(FATAL, !command_line.HasSwitch(kSwitchContentVersion))
      << "content_version must be present if version_output is";
  std::string content_version =
      command_line.GetSwitchValueASCII(kSwitchContentVersion);
  subresource_filter::WriteVersionMetadata(
      command_line.GetSwitchValuePath(kSwitchVersionOutput), content_version,
      checksum);
  return 0;
}
