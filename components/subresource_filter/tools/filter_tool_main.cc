// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#include "components/subresource_filter/tools/filter_tool.h"

namespace {

// If you change any of the switch strings, update the kHelpMsg accordingly.
const char kSwitchRuleset[] = "ruleset";
const char kSwitchOrigin[] = "document_origin";
const char kSwitchUrl[] = "url";
const char kSwitchType[] = "type";
const char kSwitchInputFile[] = "input_file";
const char kSwitchMinMatches[] = "min_matches";

const char kMatchCommand[] = "match";
const char kMatchRulesCommand[] = "match_rules";
const char kMatchBatchCommand[] = "match_batch";

const char kHelpMsg[] = R"(
  subresource_filter_tool --ruleset=<indexed_ruleset_path> command

  subresource_filter_tool is a utility for querying a ruleset, and provides
  multiple commands:

    * match --document-origin=<origin> --url=<request_url> --type=<request_type>
        Prints if the request would be blocked or allowed, as well as a
        matching ruleset rule (if one matches). The output format is:
            <BLOCKED/ALLOWED> <UrlRule if any> <document_origin> <request_url>
            <type>

        For a given request if a whitelist rule matches as well as a blacklist
        rule, the whitelist rule is printed but not the blacklist rule.

    * match_batch [--input_file=<json_file_path>]
        Like match, except it does the same for each request in stdin. A json
        file path may be provided to use in place of stdin. The input format
        is one json expression per line. An example line follows (note: in
        the file/input stream it wouldn't have a line break like this comment
        does):

        {"origin":"http://www.example.com/","request_url":"http://www.exam
        ple.com/foo.js","request_type":"script"}

    * match_rules [--input_file=<json_file_path>] [--min_matches=<optional>]
        For each record in the input (see match_batch for input formats),
        records the matching rule (see match command above) and prints all of
        the matched rules and the number of times they matched at the end.

        Which rules get recorded:
        If only a blacklist rule(s) matches, a blacklist rule is
        returned (chosen at random from list of matching blacklist rules). If
        both blacklist and whitelist rules match, a whitelist rule is
        returned. If only a whitelist rule matches, it's not recorded.

        |min_matches| is the minimum number of times the rule has to be
        matched to be included in the output. If not specified, the default is
        1.
)";

void PrintHelp() {
  printf("%s\n\n", kHelpMsg);
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringVector args = command_line.GetArgs();

  if (args.size() != 1U) {
    PrintHelp();
    return 1;
  }

  if (!command_line.HasSwitch(kSwitchRuleset)) {
    PrintHelp();
    return 1;
  }

  base::File rules_file(command_line.GetSwitchValuePath(kSwitchRuleset),
                        base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!rules_file.IsValid()) {
    std::cerr << "Could not open file: "
              << command_line.GetSwitchValueASCII(kSwitchRuleset) << std::endl;
    PrintHelp();
    return 1;
  }

  auto ruleset = subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
      std::move(rules_file));
  LOG_IF(FATAL, ruleset == nullptr) << "mmap failure";

  LOG_IF(FATAL, ruleset->length() == 0u) << "Empty ruleset file";

  subresource_filter::FilterTool filter_tool(std::move(ruleset), &std::cout);

  std::string cmd;
#if defined(OS_WIN)
  cmd = base::UTF16ToASCII(args[0]);
#else
  cmd = args[0];
#endif

  if (cmd != kMatchCommand && cmd != kMatchRulesCommand &&
      cmd != kMatchBatchCommand) {
    std::cerr << "Not a recognized command " << cmd << std::endl;
    PrintHelp();
    return 1;
  }

  if (cmd == kMatchCommand) {
    if (!command_line.HasSwitch(kSwitchOrigin) ||
        !command_line.HasSwitch(kSwitchUrl) ||
        !command_line.HasSwitch(kSwitchType)) {
      std::cerr << "Missing argument for match command:" << std::endl;
      PrintHelp();
      return 1;
    }

    const std::string document_origin =
        command_line.GetSwitchValueASCII(kSwitchOrigin);
    const std::string url = command_line.GetSwitchValueASCII(kSwitchUrl);
    const std::string type = command_line.GetSwitchValueASCII(kSwitchType);

    filter_tool.Match(document_origin, url, type);

    return 0;
  }

  int min_match_count = 0;
  if (command_line.HasSwitch(kSwitchMinMatches) &&
      !base::StringToInt(command_line.GetSwitchValueASCII(kSwitchMinMatches),
                         &min_match_count)) {
    std::cerr << "Could not convert min matches to integer: "
              << command_line.GetSwitchValueASCII(kSwitchMinMatches)
              << std::endl;
    PrintHelp();
    return 1;
  }

  std::ifstream requests_stream;
  std::istream* input_stream = &std::cin;
  if (command_line.HasSwitch(kSwitchInputFile)) {
    requests_stream =
        std::ifstream(command_line.GetSwitchValueASCII(kSwitchInputFile));
    input_stream = &requests_stream;
  }

  if (cmd == kMatchBatchCommand) {
    filter_tool.MatchBatch(input_stream);
  } else if (cmd == kMatchRulesCommand) {
    filter_tool.MatchRules(input_stream, min_match_count);
  }

  return 0;
}
