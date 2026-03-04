// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_URL_FILTER_SUGGESTION_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_URL_FILTER_SUGGESTION_H_

#include <string>

#include "url/gurl.h"

namespace multistep_filter {

// A class to hold the data for a URL based filter suggestion.
class UrlFilterSuggestion {
 public:
  explicit UrlFilterSuggestion(std::string text, GURL url)
      : text_(std::move(text)), url_(std::move(url)) {}
  ~UrlFilterSuggestion() = default;

  const std::string& text() const { return text_; }

  const GURL& url() const { return url_; }

  std::string ToString() const {
    return "UrlFilterSuggestion(text=" + text_ + ", url=" + url_.spec() + ")";
  }

  friend bool operator==(const UrlFilterSuggestion&,
                         const UrlFilterSuggestion&) = default;

 private:
  // The text content of the suggestion.
  std::string text_;
  // The URL to navigate to when the suggestion is applied.
  GURL url_;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_URL_FILTER_SUGGESTION_H_
