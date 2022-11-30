// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULE_STREAM_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULE_STREAM_H_

#include <istream>
#include <memory>
#include <ostream>

#include "components/subresource_filter/tools/ruleset_converter/ruleset_format.h"
#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {

class RuleInputStream;
class RuleOutputStream;

// Interface to read the rules one by one.
class RuleInputStream {
 public:
  virtual ~RuleInputStream() = default;

  // Fetches the next rule and returns its type. Returns RULE_TYPE_UNSPECIFIED
  // if end of the stream is reached or an I/O error occurred.
  // TODO(pkalinnikov): Distinguish between errors and end-of-stream.
  virtual url_pattern_index::proto::RuleType FetchNextRule() = 0;

  // Returns the latest fetched UrlRule. If the last FetchNextRule() returned
  // not url_pattern_index::proto::RULE_TYPE_URL, the resulting value is
  // undefined. This method is idempotent and optional to call.
  virtual url_pattern_index::proto::UrlRule GetUrlRule() = 0;

  // Same as above, but for CSS rules.
  virtual url_pattern_index::proto::CssRule GetCssRule() = 0;

  // Factory method to produce a RuleInputStream reading rules from |input| in
  // the specified |format|. If the |format| is not supported, then returns
  // nullptr.
  static std::unique_ptr<RuleInputStream> Create(
      std::unique_ptr<std::istream> input,
      RulesetFormat format);
};

// Interface to output rules one by one.
class RuleOutputStream {
 public:
  virtual ~RuleOutputStream() = default;

  // The following methods are used to write rules into the stream. Return false
  // iff an error occurred.
  virtual bool PutUrlRule(const url_pattern_index::proto::UrlRule& rule) = 0;
  virtual bool PutCssRule(const url_pattern_index::proto::CssRule& rule) = 0;

  // Finalizes the serialization. Returns false on error.
  virtual bool Finish() = 0;

  // Factory method to produce a RuleOutputStream writing to the |output| in the
  // specified |format|. If the |format| is not supported, then returns nullptr.
  static std::unique_ptr<RuleOutputStream> Create(
      std::unique_ptr<std::ostream> output,
      RulesetFormat format);
};

// Reads rules from the |input| stream, puts URL rules to |url_rules_output|,
// and CSS rules to |css_rules_output|. Returns false iff an error occurred
// either in the input or one of the output streams.
//
// If one of the output streams is nullptr, the corresponding rules are
// discarded. The same instance can be passed in for both output streams, but in
// this case the original order of rules may be not preserved. However, it is
// guaranteed that for a certain type of rules (e.g., URL) the relative order of
// such rules is preserved.
//
// If |chrome_version| is non-zero, some rules and element type options will be
// filtered out so that the output ruleset can be consumed by Chrome clients as
// early as the specified |chrome_version|. A value of 0 indicates that the
// ruleset should remain intact.
bool TransferRules(RuleInputStream* input,
                   RuleOutputStream* url_rules_output,
                   RuleOutputStream* css_rules_output,
                   int chrome_version = 0);

// This function is used by TransferRules to amend a stream of UrlRules
// according to the given |lowest_chrome_version| which uses the produced
// ruleset. Exposed here only for testing purposes.
//
// Returns false if the |rule| should be deleted altogether, otherwise returns
// true and amends the |rule| if necessary for the given
// |lowest_chrome_version|.
bool DeleteUrlRuleOrAmend(url_pattern_index::proto::UrlRule* rule,
                          int lowest_chrome_version);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULESET_CONVERTER_RULE_STREAM_H_
