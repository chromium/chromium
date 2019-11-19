// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/header.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "components/domain_reliability/config.h"

namespace {

// Parses directives in the format ("foo; bar=value for bar; baz; quux=123")
// used by NEL.
class DirectiveHeaderValueParser {
 public:
  enum State {
    BEFORE_NAME,
    AFTER_NAME,
    BEFORE_VALUE,
    AFTER_DIRECTIVE,
    SYNTAX_ERROR
  };

  explicit DirectiveHeaderValueParser(base::StringPiece value)
      : value_(value.as_string()),
        tokenizer_(value_.begin(), value_.end(), ";= "),
        stopped_with_error_(false) {
    tokenizer_.set_options(base::StringTokenizer::RETURN_DELIMS);
    tokenizer_.set_quote_chars("\"'");
  }

  // Gets the next directive, if there is one. Returns whether there was one.
  bool GetNext() {
    if (stopped_with_error_)
      return false;

    directive_name_ = base::StringPiece();
    directive_has_value_ = false;
    directive_values_.clear();

    State state = BEFORE_NAME;
    while (state != AFTER_DIRECTIVE && state != SYNTAX_ERROR
           && tokenizer_.GetNext()) {
      if (*tokenizer_.token_begin() == ' ')
        continue;

      switch (state) {
        case BEFORE_NAME:
          state = DoBeforeName();
          break;
        case AFTER_NAME:
          state = DoAfterName();
          break;
        case BEFORE_VALUE:
          state = DoBeforeValue();
          break;
        case AFTER_DIRECTIVE:
        case SYNTAX_ERROR:
          NOTREACHED();
          break;
      }
    }

    switch (state) {
      // If the parser just read the last directive, it may be in one of these
      // states, so return true to yield that directive.
      case AFTER_NAME:
      case BEFORE_VALUE:
      case AFTER_DIRECTIVE:
        return true;

      // If the parser never found a name, return false, since it doesn't have
      // a new directive for the caller.
      case BEFORE_NAME:
        return false;

      case SYNTAX_ERROR:
        stopped_with_error_ = true;
        return false;

      default:
        NOTREACHED();
        return false;
    }
  }


  base::StringPiece directive_name() const { return directive_name_; }
  bool directive_has_value() const { return directive_has_value_; }
  const std::vector<base::StringPiece>& directive_values() const {
    return directive_values_;
  }
  bool stopped_with_error() const { return stopped_with_error_; }

 private:
  State DoBeforeName() {
    if (tokenizer_.token_is_delim())
      return SYNTAX_ERROR;

    directive_name_ = tokenizer_.token_piece();
    return AFTER_NAME;
  }

  State DoAfterName() {
    if (tokenizer_.token_is_delim()) {
      char token_begin = *tokenizer_.token_begin();
      // Name can be followed by =value, ;, or just EOF.
      if (token_begin == '=') {
        directive_has_value_ = true;
        return BEFORE_VALUE;
      }
      if (token_begin == ';')
        return AFTER_DIRECTIVE;
    }
    return SYNTAX_ERROR;
  }

  State DoBeforeValue() {
    if (tokenizer_.token_is_delim()) {
      char token_begin = *tokenizer_.token_begin();
      if (token_begin == ';')
        return AFTER_DIRECTIVE;
      return SYNTAX_ERROR;
    }

    directive_values_.push_back(tokenizer_.token_piece());
    return BEFORE_VALUE;
  }

  // TODO(https://crbug.com/820198): This could take a StringPiece once
  // StringTokenizer is made StringPiece-friendly.
  std::string value_;
  base::StringTokenizer tokenizer_;

  base::StringPiece directive_name_;
  bool directive_has_value_;
  std::vector<base::StringPiece> directive_values_;
  bool stopped_with_error_;
};

bool Unquote(const std::string& in, std::string* out) {
  char first = in[0];
  char last = in[in.length() - 1];

  if (((first == '"') ^ (last == '"')) || ((first == '<') ^ (last == '>')))
    return false;

  if ((first == '"') || (first == '<'))
    *out = in.substr(1, in.length() - 2);
  else
    *out = in;
  return true;
}

bool ParseReportUri(const std::vector<base::StringPiece> in,
                    std::vector<std::unique_ptr<GURL>>* out) {
  if (in.size() < 1u)
    return false;

  out->clear();
  for (const auto& in_token : in) {
    std::string unquoted;
    if (!Unquote(in_token.as_string(), &unquoted))
      return false;
    GURL url(unquoted);
    if (!url.is_valid() || !url.SchemeIsCryptographic())
      return false;
    out->push_back(std::make_unique<GURL>(url));
  }

  return true;
}

bool ParseMaxAge(const std::vector<base::StringPiece> in,
                 base::TimeDelta* out) {
  if (in.size() != 1u)
    return false;

  int64_t seconds;
  if (!base::StringToInt64(in[0], &seconds))
    return false;

  if (seconds < 0)
    return false;

  *out = base::TimeDelta::FromSeconds(seconds);
  return true;
}

}  // namespace

namespace domain_reliability {

DomainReliabilityHeader::~DomainReliabilityHeader() {}

// static
std::unique_ptr<DomainReliabilityHeader> DomainReliabilityHeader::Parse(
    base::StringPiece value) {
  std::vector<std::unique_ptr<GURL>> report_uri;
  base::TimeDelta max_age;
  bool include_subdomains = false;

  bool got_report_uri = false;
  bool got_max_age = false;
  bool got_include_subdomains = false;

  DirectiveHeaderValueParser parser(value);
  while (parser.GetNext()) {
    base::StringPiece name = parser.directive_name();
    if (name == "report-uri") {
      if (got_report_uri
          || !parser.directive_has_value()
          || !ParseReportUri(parser.directive_values(), &report_uri)) {
        return base::WrapUnique(new DomainReliabilityHeader(PARSE_ERROR));
      }
      got_report_uri = true;
    } else if (name == "max-age") {
      if (got_max_age
          || !parser.directive_has_value()
          || !ParseMaxAge(parser.directive_values(), &max_age)) {
        return base::WrapUnique(new DomainReliabilityHeader(PARSE_ERROR));
      }
      got_max_age = true;
    } else if (name == "includeSubdomains") {
      if (got_include_subdomains ||
          parser.directive_has_value()) {
        return base::WrapUnique(new DomainReliabilityHeader(PARSE_ERROR));
      }
      include_subdomains = true;
      got_include_subdomains = true;
    } else {
      DLOG(WARNING) << "Ignoring unknown NEL header directive " << name << ".";
    }
  }

  if (parser.stopped_with_error() || !got_max_age)
    return base::WrapUnique(new DomainReliabilityHeader(PARSE_ERROR));

  if (max_age == base::TimeDelta::FromMicroseconds(0))
    return base::WrapUnique(new DomainReliabilityHeader(PARSE_CLEAR_CONFIG));

  if (!got_report_uri)
    return base::WrapUnique(new DomainReliabilityHeader(PARSE_ERROR));

  std::unique_ptr<DomainReliabilityConfig> config(
      new DomainReliabilityConfig());
  config->include_subdomains = include_subdomains;
  config->collectors.clear();
  config->collectors.swap(report_uri);
  config->success_sample_rate = 0.0;
  config->failure_sample_rate = 1.0;
  config->path_prefixes.clear();
  return base::WrapUnique(new DomainReliabilityHeader(
      PARSE_SET_CONFIG, std::move(config), max_age));
}

const DomainReliabilityConfig& DomainReliabilityHeader::config() const {
  DCHECK_EQ(PARSE_SET_CONFIG, status_);
  return *config_;
}

base::TimeDelta DomainReliabilityHeader::max_age() const {
  DCHECK_EQ(PARSE_SET_CONFIG, status_);
  return max_age_;
}

std::unique_ptr<DomainReliabilityConfig>
DomainReliabilityHeader::ReleaseConfig() {
  DCHECK_EQ(PARSE_SET_CONFIG, status_);
  status_ = PARSE_ERROR;
  return std::move(config_);
}

std::string DomainReliabilityHeader::ToString() const {
  DCHECK_EQ(PARSE_SET_CONFIG, status_);
  std::string string;
  int64_t max_age_s = max_age_.InSeconds();

  if (config_->collectors.empty()) {
    DCHECK_EQ(0, max_age_s);
  } else {
    string += "report-uri=";
    for (const auto& uri : config_->collectors)
      string += uri->spec() + " ";
    // Remove trailing space.
    string.erase(string.length() - 1, 1);
    string += "; ";
  }

  string += "max-age=" + base::NumberToString(max_age_s) + "; ";

  if (config_->include_subdomains)
    string += "includeSubdomains; ";

  // Remove trailing "; ".
  string.erase(string.length() - 2, 2);

  return string;
}

DomainReliabilityHeader::DomainReliabilityHeader(ParseStatus status)
    : status_(status) {
  DCHECK_NE(PARSE_SET_CONFIG, status_);
}

DomainReliabilityHeader::DomainReliabilityHeader(
    ParseStatus status,
    std::unique_ptr<DomainReliabilityConfig> config,
    base::TimeDelta max_age)
    : status_(status), config_(std::move(config)), max_age_(max_age) {
  DCHECK_EQ(PARSE_SET_CONFIG, status_);
  DCHECK(config_);
  DCHECK_NE(0, max_age_.InMicroseconds());
}

}  // namespace domain_reliability
