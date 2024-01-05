// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/ruleset_converter.h"

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"
#include "components/subresource_filter/tools/ruleset_converter/ruleset_format.h"

namespace subresource_filter {

RulesetConverter::RulesetConverter() = default;
RulesetConverter::~RulesetConverter() = default;

bool RulesetConverter::Convert() {
  if (inputs_.empty()) {
    std::fprintf(stderr, "No input files specified\n");
    return false;
  }

  std::unique_ptr<RuleOutputStream> primary_output;
  std::unique_ptr<RuleOutputStream> secondary_output;
  subresource_filter::RuleOutputStream* css_rules_output = nullptr;

  auto make_output = [](const base::FilePath& path, RulesetFormat format) {
    return RuleOutputStream::Create(
        std::make_unique<std::ofstream>(path.AsUTF8Unsafe(),
                                        std::ios::binary | std::ios::out),
        format);
  };

  if (!output_file_.empty()) {
    primary_output = make_output(output_file_, output_format_);
    css_rules_output = primary_output.get();
  } else {
    if (!output_url_.empty()) {
      primary_output = make_output(output_url_, output_format_);
    }
    if (output_css_ == output_url_) {
      css_rules_output = primary_output.get();
    } else if (!output_css_.empty()) {
      secondary_output = make_output(output_css_, output_format_);
      css_rules_output = secondary_output.get();
    }
  }

  if (!primary_output && !secondary_output) {
    std::fprintf(stderr,
                 "Must specify an output_file, or one of "
                 "output_file_url|output_file_css\n");
    return false;
  }

  // Iterate through input files and stream them to the outputs.
  for (const auto& path : inputs_) {
    auto input_stream = subresource_filter::RuleInputStream::Create(
        std::make_unique<std::ifstream>(path.AsUTF8Unsafe(),
                                        std::ios::binary | std::ios::in),
        input_format_);
    CHECK(input_stream);
    CHECK(TransferRules(input_stream.get(), primary_output.get(),
                        css_rules_output, chrome_version_));
  }

  if (primary_output)
    CHECK(primary_output->Finish());
  if (secondary_output)
    CHECK(secondary_output->Finish());
  return true;
}

bool RulesetConverter::SetInputFiles(
    const base::CommandLine::StringType& comma_separated_paths) {
#if BUILDFLAG(IS_WIN)
  std::wstring separatorw = L",";
  std::wstring_view separator(separatorw);
#else
  std::string_view separator(",");
#endif
  for (const auto& piece : base::SplitStringPiece(
           comma_separated_paths, separator, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path(piece);

    if (!base::PathExists(path)) {
      std::fprintf(stderr, "Path not found: %s\n", path.AsUTF8Unsafe().c_str());
      return false;
    }

    inputs_.push_back(path);
  }

  if (inputs_.empty()) {
    std::fprintf(stderr, "Received no input files\n");
    return false;
  }
  return true;
}

bool RulesetConverter::SetChromeVersion(const std::string& version) {
  int parsed_version = 0;
  if (!base::StringToInt(version, &parsed_version)) {
    std::fprintf(stderr,
                 "chrome_version could not be parsed into an integer.\n");
    return false;
  }
  if (parsed_version != 0 && parsed_version != 54 && parsed_version != 59) {
    std::fprintf(stderr, "chrome_version should be in {0, 54, 59}.\n");
    return false;
  }
  chrome_version_ = parsed_version;
  return true;
}

bool RulesetConverter::SetOutputFile(const base::FilePath& path) {
  if (!base::DirectoryExists(path.DirName())) {
    std::printf("Directory does not exist: %s\n",
                path.DirName().AsUTF8Unsafe().c_str());
    return false;
  }
  output_file_ = path;
  return true;
}

bool RulesetConverter::SetOutputFileUrl(const base::FilePath& path) {
  if (!base::DirectoryExists(path.DirName())) {
    std::printf("Directory does not exist: %s\n",
                path.DirName().AsUTF8Unsafe().c_str());
    return false;
  }
  output_url_ = path;
  return true;
}

bool RulesetConverter::SetOutputFileCss(const base::FilePath& path) {
  if (!base::DirectoryExists(path.DirName())) {
    std::printf("Directory does not exist: %s\n",
                path.DirName().AsUTF8Unsafe().c_str());
    return false;
  }
  output_css_ = path;
  return true;
}

bool RulesetConverter::SetInputFormat(const std::string& format) {
  RulesetFormat ruleset_format = ParseFlag(format);
  if (ruleset_format == subresource_filter::RulesetFormat::kUndefined) {
    std::fprintf(stderr, "Input format is not defined.\n");
    return false;
  }
  input_format_ = ruleset_format;
  return true;
}

bool RulesetConverter::SetOutputFormat(const std::string& format) {
  RulesetFormat ruleset_format = ParseFlag(format);
  if (ruleset_format == subresource_filter::RulesetFormat::kUndefined) {
    std::fprintf(stderr, "Output format is not defined.\n");
    return false;
  }
  output_format_ = ruleset_format;
  return true;
}

}  // namespace subresource_filter
