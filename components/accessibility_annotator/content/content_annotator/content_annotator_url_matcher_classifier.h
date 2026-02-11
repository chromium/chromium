// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_URL_MATCHER_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_URL_MATCHER_CLASSIFIER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/pass_key.h"
#include "components/url_matcher/url_matcher.h"

namespace accessibility_annotator {

// A classifier that uses a UrlMatcher to classify URLs.
class ContentAnnotatorUrlMatcherClassifier {
 public:
  using PassKey = base::PassKey<ContentAnnotatorUrlMatcherClassifier>;

  // Creates a URL matcher classifier from a string of rules.
  // Matching is done using by comparing the URL to the regex rules.
  // Returns nullptr if the rules are invalid.
  static std::unique_ptr<ContentAnnotatorUrlMatcherClassifier> Create(
      std::string_view rules_json);

  explicit ContentAnnotatorUrlMatcherClassifier(
      PassKey pass_key,
      std::unique_ptr<url_matcher::URLMatcher> url_matcher,
      base::flat_map<base::MatcherStringPattern::ID, std::string>
          id_to_category_map);
  ~ContentAnnotatorUrlMatcherClassifier();
  ContentAnnotatorUrlMatcherClassifier(
      const ContentAnnotatorUrlMatcherClassifier&) = delete;
  ContentAnnotatorUrlMatcherClassifier& operator=(
      const ContentAnnotatorUrlMatcherClassifier&) = delete;
  ContentAnnotatorUrlMatcherClassifier(ContentAnnotatorUrlMatcherClassifier&&);
  ContentAnnotatorUrlMatcherClassifier& operator=(
      ContentAnnotatorUrlMatcherClassifier&&);

  // Returns the category of the URL or std::nullopt if there is no match.
  // If multiple categories match, returns the first one.
  // TODO(crbug.com/482477208): Add support for multiple categories.
  std::optional<std::string_view> Classify(const GURL& url) const;

 private:
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
  base::flat_map<base::MatcherStringPattern::ID, std::string>
      id_to_category_map_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_URL_MATCHER_CLASSIFIER_H_
