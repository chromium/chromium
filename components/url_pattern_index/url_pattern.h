// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PATTERN_INDEX_URL_PATTERN_H_
#define COMPONENTS_URL_PATTERN_INDEX_URL_PATTERN_H_

#include <iosfwd>
#include <optional>
#include <string_view>

#include "components/url_pattern_index/proto/rules.pb.h"
#include "url/third_party/mozilla/url_parse.h"

class GURL;

namespace url_pattern_index {

namespace flat {
struct UrlRule;  // The FlatBuffers version of UrlRule.
}

// The structure used to mirror a URL pattern regardless of the representation
// of the UrlRule that owns it, and to match it against URLs.
class UrlPattern {
 public:
  enum class MatchCase {
    kTrue,
    kFalse,
  };

  // A wrapper over a GURL to reduce redundant computation.
  class UrlInfo {
   public:
    // The |url| must outlive this instance.
    UrlInfo(const GURL& url);

    UrlInfo(const UrlInfo&) = delete;
    UrlInfo& operator=(const UrlInfo&) = delete;

    ~UrlInfo();

    std::string_view spec() const { return spec_; }
    std::string_view GetLowerCaseSpec() const;
    url::Component host() const { return host_; }
    std::string_view GetStringHost() const;

   private:
    // The url spec.
    const std::string_view spec_;
    // String to hold the lazily computed lower cased spec.
    mutable std::string lower_case_spec_owner_;
    // Reference to the lower case spec. Computed lazily.
    mutable std::optional<std::string_view> lower_case_spec_cached_;

    // The url host component.
    const url::Component host_;
  };

  UrlPattern();

  // Creates a |url_pattern| of a certain |type| and case-sensitivity.
  UrlPattern(std::string_view url_pattern,
             proto::UrlPatternType type = proto::URL_PATTERN_TYPE_WILDCARDED,
             MatchCase match_case = MatchCase::kFalse);

  // Creates a WILDCARDED |url_pattern| with the specified anchors.
  UrlPattern(std::string_view url_pattern,
             proto::AnchorType anchor_left,
             proto::AnchorType anchor_right);

  // The passed in |rule| must outlive the created instance.
  explicit UrlPattern(const flat::UrlRule& rule);

  UrlPattern(const UrlPattern&) = delete;
  UrlPattern& operator=(const UrlPattern&) = delete;

  ~UrlPattern();

  proto::UrlPatternType type() const { return type_; }
  std::string_view url_pattern() const { return url_pattern_; }
  proto::AnchorType anchor_left() const { return anchor_left_; }
  proto::AnchorType anchor_right() const { return anchor_right_; }
  bool match_case() const { return match_case_ == MatchCase::kTrue; }

  // Returns whether the |url| matches the URL |pattern|. Requires the type of
  // |this| pattern to be either SUBSTRING or WILDCARDED.
  //
  // Splits the pattern into subpatterns separated by '*' wildcards, and
  // greedily finds each of them in the spec of the |url|. Respects anchors at
  // either end of the pattern, and '^' separator placeholders when comparing a
  // subpattern to a subtring of the spec.
  bool MatchesUrl(const UrlInfo& url) const;

 private:
  // TODO(pkalinnikov): Store flat:: types instead of proto::, in order to avoid
  // conversions in IndexedRuleset.
  proto::UrlPatternType type_ = proto::URL_PATTERN_TYPE_UNSPECIFIED;
  std::string_view url_pattern_;

  proto::AnchorType anchor_left_ = proto::ANCHOR_TYPE_NONE;
  proto::AnchorType anchor_right_ = proto::ANCHOR_TYPE_NONE;

  MatchCase match_case_ = MatchCase::kTrue;
};

// Allow pretty-printing URLPatterns when they are used in GTest assertions.
std::ostream& operator<<(std::ostream& out, const UrlPattern& pattern);

}  // namespace url_pattern_index

#endif  // COMPONENTS_URL_PATTERN_INDEX_URL_PATTERN_H_
