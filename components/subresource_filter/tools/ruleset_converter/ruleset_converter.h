// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULESET_CONVERTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULESET_CONVERTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"
#include "components/subresource_filter/tools/ruleset_converter/ruleset_format.h"

namespace subresource_filter {

// The RulesetConverter converts subresource_filter rulesets across multiple
// formats.
// This class is a thin abstraction to enable testing of the |ruleset_converter|
// command line tool. See comments in main.cc for more information.
class RulesetConverter {
 public:
  RulesetConverter();

  RulesetConverter(const RulesetConverter&) = delete;
  RulesetConverter& operator=(const RulesetConverter&) = delete;

  ~RulesetConverter();

  // Converts rulesets based on Set* configurations.
  bool Convert();

  // Returns false if the input files are invalid or cannot be found.
  // Corresponds to --input_files parameter.
  bool SetInputFiles(
      const base::CommandLine::StringType& comma_separated_paths);

  // These methods will return false if the directory does not exist.
  //
  // Corresponds to --output_file parameter.
  bool SetOutputFile(const base::FilePath& path);

  // Corresponds to --output_file_url parameter.
  bool SetOutputFileUrl(const base::FilePath& path);

  // Corresponds to --output_file_css parameter.
  bool SetOutputFileCss(const base::FilePath& path);

  // Corresponds to --chrome_version.
  bool SetChromeVersion(const std::string& version);

  // Corresponds to --input_format / --output_format.
  bool SetInputFormat(const std::string& format);
  bool SetOutputFormat(const std::string& format);

 private:
  std::vector<base::FilePath> inputs_;

  base::FilePath output_file_;
  base::FilePath output_url_;
  base::FilePath output_css_;

  RulesetFormat input_format_ = RulesetFormat::kFilterList;
  RulesetFormat output_format_ = RulesetFormat::kUnindexedRuleset;

  // Increase this if rule_stream gets more custom logic for versions > 59.
  int chrome_version_ = 59;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULESET_CONVERTER_H_
