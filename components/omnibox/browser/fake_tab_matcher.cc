// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/omnibox/browser/fake_tab_matcher.h"

bool FakeTabMatcher::IsTabOpenWithURL(const GURL& url,
                                      const AutocompleteInput* input) const {
  return !substring_to_match_.empty() &&
         url.spec().find(substring_to_match_) != std::string::npos;
}
