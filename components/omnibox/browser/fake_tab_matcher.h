// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_TAB_MATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_TAB_MATCHER_H_

#include <string>

#include "components/omnibox/browser/tab_matcher.h"

// Fake implementation of TabMatcher, allowing arbitrary string matching for use
// with tests.
class FakeTabMatcher : public TabMatcher {
 public:
  // A test calls this to establish the set of URLs that will return
  // true from IsTabOpenWithURL() above. It's a simple substring match
  // of the URL.
  void set_url_substring_match(const std::string& substr) {
    substring_to_match_ = substr;
  }

  bool IsTabOpenWithURL(const GURL& url,
                        const AutocompleteInput* input) const override {
    return !substring_to_match_.empty() &&
           url.spec().find(substring_to_match_) != std::string::npos;
  }

 private:
  // Substring used to match URLs for IsTabOpenWithURL().
  std::string substring_to_match_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_TAB_MATCHER_H_
