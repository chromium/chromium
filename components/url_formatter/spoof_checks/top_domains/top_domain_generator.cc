// Copyright 2018 The Chromium Authors
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
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_state_generator.h"
#include "components/url_formatter/spoof_checks/top_domains/trie_entry.h"
#include "url/gurl.h"

using url_formatter::top_domains::TopDomainEntries;
using url_formatter::top_domains::TopDomainEntry;
using url_formatter::top_domains::TopDomainStateGenerator;

namespace {

const char* kTopBucketSeparator = "###END_TOP_BUCKET###";

// Print the command line help.
void PrintHelp() {
  std::cout << "top_domain_generator <input-file>"
            << " <template-file> <output-file> [--v=1]" << std::endl;
}

void CheckName(const std::string& name) {
  for (char c : name) {
    CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || c == '.' || c == '-' || c == '_')
        << name << " has invalid characters.";
  }
}

std::unique_ptr<TopDomainEntry> MakeEntry(
    const std::string& hostname,
    const std::string& skeleton,
    url_formatter::SkeletonType skeleton_type,
    bool is_top_bucket,
    std::set<std::string>* all_skeletons) {
  auto entry = std::make_unique<TopDomainEntry>();
  // Another site has the same skeleton. This is low proability so stop now.
  CHECK(all_skeletons->find(skeleton) == all_skeletons->end())
      << "A domain with the same skeleton is already in the list (" << skeleton
      << ").";

  all_skeletons->insert(skeleton);

  // TODO: Should we lowercase these?
  entry->skeleton = skeleton;

  // There might be unicode domains in the list. Store them in punycode in
  // the trie.
  const GURL domain(std::string("http://") + hostname);
  entry->top_domain = domain.host();

  entry->is_top_bucket = is_top_bucket;
  entry->skeleton_type = skeleton_type;

  CheckName(entry->skeleton);
  CheckName(entry->top_domain);

  return entry;
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

  base::i18n::InitializeICU();

#if BUILDFLAG(IS_WIN)
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

  std::vector<std::string> lines = base::SplitString(
      input_text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  bool is_top_bucket = true;
  TopDomainEntries entries;
  std::set<std::string> all_skeletons;
  for (std::string line : lines) {
    base::TrimWhitespaceASCII(line, base::TRIM_ALL, &line);

    if (line == kTopBucketSeparator) {
      is_top_bucket = false;
      continue;
    }

    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::vector<std::string> tokens = base::SplitString(
        line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    // Domains in the top bucket will have full skeletons as well as skeletons
    // without label separators (e.g. '.' and '-').
    if (is_top_bucket) {
      CHECK_EQ(3u, tokens.size()) << "Invalid line: " << tokens[0];

      entries.push_back(MakeEntry(tokens[2], tokens[0],
                                  url_formatter::SkeletonType::kFull,
                                  /*is_top_bucket=*/true, &all_skeletons));
      entries.push_back(MakeEntry(
          tokens[2], tokens[1], url_formatter::SkeletonType::kSeparatorsRemoved,
          /*is_top_bucket=*/true, &all_skeletons));
    } else {
      CHECK_EQ(2u, tokens.size()) << "Invalid line: " << tokens[0];

      entries.push_back(MakeEntry(tokens[1], tokens[0],
                                  url_formatter::SkeletonType::kFull,
                                  /*is_top_bucket=*/false, &all_skeletons));
    }
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
  if (!base::WriteFile(output_path, output)) {
    LOG(ERROR) << "Failed to write output: " << output_path;
    return 1;
  }

  return 0;
}
