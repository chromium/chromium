// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TAB_MATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_TAB_MATCHER_H_

#include "components/omnibox/browser/autocomplete_input.h"
#include "url/gurl.h"

// Abstraction of a mechanism that associates GURL objects with open tabs.
class TabMatcher {
 public:
  TabMatcher() = default;
  TabMatcher(TabMatcher&&) = delete;
  TabMatcher(const TabMatcher&) = delete;
  TabMatcher& operator=(TabMatcher&&) = delete;
  TabMatcher& operator=(const TabMatcher&) = delete;

  virtual ~TabMatcher() = default;

  // For a given URL, check if a tab already exists where that URL is already
  // opened.
  // Returns true, if the URL can be matched to existing tab, otherwise false.
  virtual bool IsTabOpenWithURL(const GURL& gurl,
                                const AutocompleteInput* input) const = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TAB_MATCHER_H_
