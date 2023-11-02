// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "components/subresource_filter/tools/rule_parser/rule.h"
#include "components/subresource_filter/tools/rule_parser/rule_parser.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

namespace {

std::string ReadStreamToString(std::istream* input) {
  return std::string(std::istreambuf_iterator<char>(*input), {});
}

bool IsTrivialParseError(const RuleParser::ParseError& error) {
  return error.error_code == RuleParser::ParseError::NONE ||
         error.error_code == RuleParser::ParseError::EMPTY_RULE;
}

// A helper class used by rule input streams for converting a FilteringRules
// message into a stream.
class ProtobufRuleInputStreamImpl {
 public:
  explicit ProtobufRuleInputStreamImpl(
      const url_pattern_index::proto::FilteringRules& rules)
      : rules_(rules) {}

  url_pattern_index::proto::RuleType FetchNextRule() {
    if (not_first_rule_)
      ++rule_index_;
    not_first_rule_ = true;

    if (is_reading_url_rules_ && rule_index_ >= rules_.url_rules_size()) {
      is_reading_url_rules_ = false;
      rule_index_ = 0;
    }
    if (!is_reading_url_rules_ && rule_index_ >= rules_.css_rules_size())
      return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
    return is_reading_url_rules_ ? url_pattern_index::proto::RULE_TYPE_URL
                                 : url_pattern_index::proto::RULE_TYPE_CSS;
  }

  const url_pattern_index::proto::UrlRule& GetUrlRule() {
    CHECK(is_reading_url_rules_ && rule_index_ < rules_.url_rules_size());
    return rules_.url_rules(rule_index_);
  }

  const url_pattern_index::proto::CssRule& GetCssRule() {
    CHECK(!is_reading_url_rules_ && rule_index_ < rules_.css_rules_size());
    return rules_.css_rules(rule_index_);
  }

 private:
  ProtobufRuleInputStreamImpl(const ProtobufRuleInputStreamImpl&) = delete;
  void operator=(const ProtobufRuleInputStreamImpl&) = delete;

  const url_pattern_index::proto::FilteringRules& rules_;
  bool not_first_rule_ = false;
  bool is_reading_url_rules_ = true;
  int rule_index_ = 0;
};

// FilterList streams ---------------------------------------------------------

// Reads rules from a text |file| of the FilterList format.
class FilterListRuleInputStream : public RuleInputStream {
 public:
  explicit FilterListRuleInputStream(std::unique_ptr<std::istream> input)
      : input_(std::move(input)) {}

  url_pattern_index::proto::RuleType FetchNextRule() override {
    std::string line;
    while (std::getline(*input_, line)) {
      auto rule_type = parser_.Parse(line);
      if (rule_type != url_pattern_index::proto::RULE_TYPE_UNSPECIFIED)
        return rule_type;
      if (!IsTrivialParseError(parser_.parse_error())) {
        LOG(ERROR) << parser_.parse_error();
      }
      // TODO(pkalinnikov): Export the number of processed/skipped rules.
    }
    return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  }

  url_pattern_index::proto::UrlRule GetUrlRule() override {
    CHECK_EQ(url_pattern_index::proto::RULE_TYPE_URL, parser_.rule_type());
    return parser_.url_rule().ToProtobuf();
  }

  url_pattern_index::proto::CssRule GetCssRule() override {
    CHECK_EQ(url_pattern_index::proto::RULE_TYPE_CSS, parser_.rule_type());
    return parser_.css_rule().ToProtobuf();
  }

 private:
  FilterListRuleInputStream(const FilterListRuleInputStream&) = delete;
  void operator=(const FilterListRuleInputStream&) = delete;

  RuleParser parser_;
  std::unique_ptr<std::istream> input_;
};

// Writes rules to |output| in FilterList format.
class FilterListRuleOutputStream : public RuleOutputStream {
 public:
  explicit FilterListRuleOutputStream(std::unique_ptr<std::ostream> output)
      : output_(std::move(output)) {}

  bool PutUrlRule(const url_pattern_index::proto::UrlRule& rule) override {
    std::string line = ToString(rule) + '\n';
    output_->write(line.data(), line.size());
    return !output_->bad();
  }

  bool PutCssRule(const url_pattern_index::proto::CssRule& rule) override {
    std::string line = ToString(rule) + '\n';
    output_->write(line.data(), line.size());
    return !output_->bad();
  }

  bool Finish() override {
    output_->flush();
    return !output_->bad();
  }

 private:
  FilterListRuleOutputStream(const FilterListRuleOutputStream&) = delete;
  void operator=(const FilterListRuleOutputStream&) = delete;

  std::unique_ptr<std::ostream> output_;
};

// Protobuf streams ------------------------------------------------------------

// Reads rules from a |file| with a serialized FilteredRules message.
class ProtobufRuleInputStream : public RuleInputStream {
 public:
  explicit ProtobufRuleInputStream(std::unique_ptr<std::istream> input) {
    std::string buffer = ReadStreamToString(input.get());
    CHECK(rules_.ParseFromString(buffer));
    impl_ = std::make_unique<ProtobufRuleInputStreamImpl>(rules_);
  }

  url_pattern_index::proto::RuleType FetchNextRule() override {
    return impl_->FetchNextRule();
  }
  url_pattern_index::proto::UrlRule GetUrlRule() override {
    return impl_->GetUrlRule();
  }
  url_pattern_index::proto::CssRule GetCssRule() override {
    return impl_->GetCssRule();
  }

 private:
  ProtobufRuleInputStream(const ProtobufRuleInputStream&) = delete;
  void operator=(const ProtobufRuleInputStream&) = delete;

  url_pattern_index::proto::FilteringRules rules_;
  std::unique_ptr<ProtobufRuleInputStreamImpl> impl_;
};

// Writes rules to |output| as a single url_pattern_index::proto::FilteringRules
// message in binary format.
class ProtobufRuleOutputStream : public RuleOutputStream {
 public:
  explicit ProtobufRuleOutputStream(std::unique_ptr<std::ostream> output)
      : output_(std::move(output)) {}

  bool PutUrlRule(const url_pattern_index::proto::UrlRule& rule) override {
    *all_rules_.add_url_rules() = rule;
    return true;
  }

  bool PutCssRule(const url_pattern_index::proto::CssRule& rule) override {
    *all_rules_.add_css_rules() = rule;
    return true;
  }

  bool Finish() override {
    std::string buffer;
    if (!all_rules_.SerializeToString(&buffer))
      return false;
    output_->write(buffer.data(), buffer.size());
    output_->flush();
    return !output_->bad();
  }

 private:
  ProtobufRuleOutputStream(const ProtobufRuleOutputStream&) = delete;
  void operator=(const ProtobufRuleOutputStream&) = delete;

  url_pattern_index::proto::FilteringRules all_rules_;
  std::unique_ptr<std::ostream> output_;
};

// UnindexedRuleset streams ----------------------------------------------------

// Reads rules stored in |input| in UnindexedRuleset format.
class UnindexedRulesetRuleInputStream : public RuleInputStream {
 public:
  explicit UnindexedRulesetRuleInputStream(
      std::unique_ptr<std::istream> input) {
    ruleset_ = ReadStreamToString(input.get());
    ruleset_input_ = std::make_unique<google::protobuf::io::ArrayInputStream>(
        ruleset_.data(), ruleset_.size());
    ruleset_reader_ =
        std::make_unique<UnindexedRulesetReader>(ruleset_input_.get());
  }

  url_pattern_index::proto::RuleType FetchNextRule() override {
    if (!impl_ && !ReadNextChunk())
      return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
    url_pattern_index::proto::RuleType rule_type = impl_->FetchNextRule();
    while (rule_type == url_pattern_index::proto::RULE_TYPE_UNSPECIFIED &&
           ReadNextChunk()) {
      rule_type = impl_->FetchNextRule();
    }
    return rule_type;
  }

  url_pattern_index::proto::UrlRule GetUrlRule() override {
    return impl_->GetUrlRule();
  }
  url_pattern_index::proto::CssRule GetCssRule() override {
    return impl_->GetCssRule();
  }

 private:
  UnindexedRulesetRuleInputStream(const UnindexedRulesetRuleInputStream&) =
      delete;
  void operator=(const UnindexedRulesetRuleInputStream&) = delete;

  bool ReadNextChunk() {
    if (ruleset_reader_->ReadNextChunk(&rules_chunk_)) {
      impl_ = std::make_unique<ProtobufRuleInputStreamImpl>(rules_chunk_);
      return true;
    }
    impl_.reset();
    return false;
  }

  std::string ruleset_;
  std::unique_ptr<google::protobuf::io::ArrayInputStream> ruleset_input_;
  std::unique_ptr<UnindexedRulesetReader> ruleset_reader_;

  url_pattern_index::proto::FilteringRules rules_chunk_;
  std::unique_ptr<ProtobufRuleInputStreamImpl> impl_;
};

// Writes the rules to |output| in UnindexedRuleset format. Discards CSS rules.
class UnindexedRulesetRuleOutputStream : public RuleOutputStream {
 public:
  explicit UnindexedRulesetRuleOutputStream(
      std::unique_ptr<std::ostream> output)
      : ruleset_output_(&ruleset_),
        ruleset_writer_(&ruleset_output_),
        output_(std::move(output)) {}

  bool PutUrlRule(const url_pattern_index::proto::UrlRule& rule) override {
    return ruleset_writer_.AddUrlRule(rule);
  }

  bool PutCssRule(const url_pattern_index::proto::CssRule& rule) override {
    return true;
  }

  bool Finish() override {
    if (!ruleset_writer_.Finish())
      return false;
    output_->write(ruleset_.data(), ruleset_.size());
    output_->flush();
    return !output_->bad();
  }

 private:
  UnindexedRulesetRuleOutputStream(const UnindexedRulesetRuleOutputStream&) =
      delete;
  void operator=(const UnindexedRulesetRuleOutputStream&) = delete;

  std::string ruleset_;
  google::protobuf::io::StringOutputStream ruleset_output_;
  UnindexedRulesetWriter ruleset_writer_;
  std::unique_ptr<std::ostream> output_;
};

}  // namespace

// static
std::unique_ptr<RuleInputStream> RuleInputStream::Create(
    std::unique_ptr<std::istream> input,
    RulesetFormat format) {
  CHECK(input);
  CHECK(!input->bad());
  std::unique_ptr<RuleInputStream> result;
  switch (format) {
    case RulesetFormat::kFilterList:
      result = std::make_unique<FilterListRuleInputStream>(std::move(input));
      break;
    case RulesetFormat::kProto:
      result = std::make_unique<ProtobufRuleInputStream>(std::move(input));
      break;
    case RulesetFormat::kUnindexedRuleset:
      result =
          std::make_unique<UnindexedRulesetRuleInputStream>(std::move(input));
      break;
    default:
      break;
  }
  return result;
}

// static
std::unique_ptr<RuleOutputStream> RuleOutputStream::Create(
    std::unique_ptr<std::ostream> output,
    RulesetFormat format) {
  CHECK(output);
  CHECK(!output->bad());
  std::unique_ptr<RuleOutputStream> result;
  switch (format) {
    case RulesetFormat::kFilterList:
      result = std::make_unique<FilterListRuleOutputStream>(std::move(output));
      break;
    case RulesetFormat::kProto:
      result = std::make_unique<ProtobufRuleOutputStream>(std::move(output));
      break;
    case RulesetFormat::kUnindexedRuleset:
      result =
          std::make_unique<UnindexedRulesetRuleOutputStream>(std::move(output));
      break;
    default:
      break;
  }
  return result;
}

// TransferRules helpers and implementation. -----------------------------------

namespace {

// Up to M58 subresource_filter supported only these types.
constexpr int kChrome54To58ElementTypes = 2047;

static_assert(kChrome54To58ElementTypes &
                  url_pattern_index::proto::ELEMENT_TYPE_OTHER,
              "Wrong M54 element types.");
static_assert(kChrome54To58ElementTypes &
                  url_pattern_index::proto::ELEMENT_TYPE_FONT,
              "Wrong M54 element types.");
static_assert(!(kChrome54To58ElementTypes &
                url_pattern_index::proto::ELEMENT_TYPE_WEBSOCKET),
              "Wrong M54 element types.");
static_assert(!(kChrome54To58ElementTypes &
                url_pattern_index::proto::ELEMENT_TYPE_WEBTRANSPORT),
              "Wrong M54 element types.");
static_assert(!(kChrome54To58ElementTypes &
                url_pattern_index::proto::ELEMENT_TYPE_WEBBUNDLE),
              "Wrong M54 element types.");
static_assert(!(kChrome54To58ElementTypes &
                url_pattern_index::proto::ELEMENT_TYPE_POPUP),
              "Wrong M54 element types.");

}  // namespace

bool TransferRules(RuleInputStream* input,
                   RuleOutputStream* url_rules_output,
                   RuleOutputStream* css_rules_output,
                   int chrome_version) {
  while (true) {
    auto rule_type = input->FetchNextRule();
    if (rule_type == url_pattern_index::proto::RULE_TYPE_UNSPECIFIED)
      break;
    switch (rule_type) {
      case url_pattern_index::proto::RULE_TYPE_URL: {
        if (!url_rules_output)
          break;
        url_pattern_index::proto::UrlRule url_rule = input->GetUrlRule();
        if (!DeleteUrlRuleOrAmend(&url_rule, chrome_version))
          url_rules_output->PutUrlRule(url_rule);
        break;
      }
      case url_pattern_index::proto::RULE_TYPE_CSS:
        if (css_rules_output)
          css_rules_output->PutCssRule(input->GetCssRule());
        break;
      case url_pattern_index::proto::RULE_TYPE_COMMENT:
        // Ignore comments.
        break;
      default:
        return false;
    }
  }
  return true;
}

bool DeleteUrlRuleOrAmend(url_pattern_index::proto::UrlRule* rule,
                          int lowest_chrome_version) {
  if (!lowest_chrome_version)
    return false;

  CHECK(rule->has_element_types() || rule->element_types() == 0);

  // REGEXP rules are not supported in Chrome's subresource_filter.
  if (rule->url_pattern_type() ==
      url_pattern_index::proto::URL_PATTERN_TYPE_REGEXP)
    return true;

  // POPUP type is deprecated because popup blocking is activated by default
  // in Chrome.
  rule->set_element_types(rule->element_types() &
                          ~url_pattern_index::proto::ELEMENT_TYPE_POPUP);

  // Only the following activation types are supported in Chrome.
  rule->set_activation_types(
      rule->activation_types() &
      (url_pattern_index::proto::ACTIVATION_TYPE_DOCUMENT |
       url_pattern_index::proto::ACTIVATION_TYPE_GENERICBLOCK));
  if (!rule->activation_types())
    rule->clear_activation_types();

  // Chrome 54-58 ignores rules with unknown element types (like websocket).
  if (lowest_chrome_version == 54) {
    // Remove unknown types to prevent the |rule| from being ignored.
    rule->set_element_types(rule->element_types() & kChrome54To58ElementTypes);
  }
  if (!rule->element_types())
    rule->clear_element_types();

  // The rule should have at least 1 type bit, otherwise it targets nothing.
  return !rule->element_types() && !rule->activation_types();
}

}  // namespace subresource_filter
