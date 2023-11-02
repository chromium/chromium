// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULESET_TEST_UTIL_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULESET_TEST_UTIL_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"
#include "components/subresource_filter/tools/ruleset_converter/ruleset_format.h"
#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {

// Stores lists of the rules that a test ruleset consists of.
struct TestRulesetContents {
  TestRulesetContents();
  TestRulesetContents(const TestRulesetContents& other);
  ~TestRulesetContents();

  std::vector<url_pattern_index::proto::UrlRule> url_rules;
  std::vector<url_pattern_index::proto::CssRule> css_rules;

  // Parses |text_rules| and appends them to the |ruleset|.
  void AppendRules(const std::vector<std::string>& text_rules,
                   bool allow_errors = false);

  // Extends this TestRulesetContents rules with rules from |other|.
  void AppendParsedRules(const TestRulesetContents& other);

  bool operator==(const TestRulesetContents& other) const;
};
std::ostream& operator<<(std::ostream& out,
                         const TestRulesetContents& contents);

// Stores identification information about a temporary File with a ruleset.
// Deletes the file automatically on destruction.
class ScopedTempRulesetFile {
 public:
  // Creates a temporary file of the specified |format|.
  explicit ScopedTempRulesetFile(RulesetFormat format);

  ScopedTempRulesetFile(const ScopedTempRulesetFile&) = delete;
  ScopedTempRulesetFile& operator=(const ScopedTempRulesetFile&) = delete;

  ~ScopedTempRulesetFile();

  // Opens the |ruleset_file| and creates an empty rule output stream to this
  // file. Returns the stream or nullptr if it failed to be created.
  std::unique_ptr<RuleOutputStream> OpenForOutput() const;

  // Opens the |ruleset_file| and returns the corresponding input stream, or
  // nullptr if it failed to be created.
  std::unique_ptr<RuleInputStream> OpenForInput() const;

  // Opens the |ruleset_file|, and writes the test ruleset |contents| to it in
  // the corresponding format.
  void WriteRuleset(const TestRulesetContents& contents) const;

  TestRulesetContents ReadContents() const;

  const base::FilePath& ruleset_path() const { return ruleset_path_; }
  RulesetFormat format() const { return format_; }

 private:
  base::ScopedTempDir scoped_dir_;
  base::FilePath ruleset_path_;
  const RulesetFormat format_;  // The format of the |file|.
};

bool AreUrlRulesEqual(const url_pattern_index::proto::UrlRule& first,
                      const url_pattern_index::proto::UrlRule& second);

bool AreCssRulesEqual(const url_pattern_index::proto::CssRule& first,
                      const url_pattern_index::proto::CssRule& second);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULESET_TEST_UTIL_H_
