// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This binary generates a Huffman encoded trie from the top domain skeleton
// list. The keys of the trie are skeletons and the values are the corresponding
// top domains.
//
// The input is the list of (skeleton, domain) pairs. The output is written
// using the given template file.

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_state_generator.h"
#include "components/url_formatter/spoof_checks/top_domains/trie_entry.h"

using url_formatter::top_domains::TopDomainEntries;
using url_formatter::top_domains::TopDomainEntry;
using url_formatter::top_domains::TopDomainStateGenerator;

namespace {

// Print the command line help.
void PrintHelp() {
  std::cout << "top_domain_generator <input-file>"
            << " <template-file> <output-file> [--for_testing] [--v=1]"
            << std::endl;
}

void CheckName(const std::string& name) {
  for (char c : name) {
    CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_')
        << name << " has invalid characters.";
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

#if defined(OS_WIN)
  std::vector<std::string> args;
  base::CommandLine::StringVector wide_args = command_line.GetArgs();
  for (const auto& arg : wide_args) {
    args.push_back(base::WideToUTF8(arg));
  }
#else
  base::CommandLine::StringVector args = command_line.GetArgs();
#endif
  if (args.size() < 3) {
    PrintHelp();
    return 1;
  }

  base::FilePath input_path =
      base::MakeAbsoluteFilePath(base::FilePath::FromUTF8Unsafe(args[0]));
  if (!base::PathExists(input_path)) {
    LOG(ERROR) << "Input path doesn't exist: " << input_path;
    return 1;
  }

  std::string input_text;
  if (!base::ReadFileToString(input_path, &input_text)) {
    LOG(ERROR) << "Could not read input file: " << input_path;
    return 1;
  }

  const bool for_testing = command_line.HasSwitch("for_testing");

  std::vector<std::string> lines = base::SplitString(
      input_text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  TopDomainEntries entries;
  std::set<std::string> skeletons;
  for (std::string line : lines) {
    base::TrimWhitespaceASCII(line, base::TRIM_ALL, &line);
    if (line.empty() || line[0] == '#') {
      continue;
    }
    auto entry = std::make_unique<TopDomainEntry>();

    std::vector<std::string> tokens = base::SplitString(
        line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    CHECK_EQ(2u, tokens.size()) << "Invalid line: " << tokens[0];
    const std::string skeleton = tokens[0];

    if (skeletons.find(skeleton) != skeletons.end()) {
      // Another site has the same skeleton. Simply ignore, as we already have a
      // top domain corresponding to this skeleton.
      continue;
    }
    skeletons.insert(skeleton);

    // TODO: Should we lowercase these?
    entry->skeleton = skeleton;
    entry->top_domain = tokens[1];

    // If testing, only mark the first 5 sites as "top 500".
    if (for_testing) {
      entry->is_top_500 = entries.size() < 1;
    } else {
      entry->is_top_500 = entries.size() < 500;
    }

    CheckName(entry->skeleton);
    CheckName(entry->top_domain);

    entries.push_back(std::move(entry));
  }

  base::FilePath template_path = base::FilePath::FromUTF8Unsafe(args[1]);
  if (!base::PathExists(template_path)) {
    LOG(ERROR) << "Template file doesn't exist: " << template_path;
    return 1;
  }
  template_path = base::MakeAbsoluteFilePath(template_path);

  std::string template_string;
  if (!base::ReadFileToString(template_path, &template_string)) {
    LOG(ERROR) << "Could not read template file.";
    return 1;
  }

  TopDomainStateGenerator generator;
  std::string output = generator.Generate(template_string, entries);
  if (output.empty()) {
    LOG(ERROR) << "Trie generation failed.";
    return 1;
  }

  base::FilePath output_path = base::FilePath::FromUTF8Unsafe(args[2]);
  if (base::WriteFile(output_path, output.c_str(),
                      static_cast<uint32_t>(output.size())) <= 0) {
    LOG(ERROR) << "Failed to write output: " << output_path;
    return 1;
  }

  return 0;
}
