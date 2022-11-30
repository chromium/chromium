// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOCK_TAB_MATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOCK_TAB_MATCHER_H_

#include "components/omnibox/browser/tab_matcher.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mock implementation of TabMatcher, allowing arbitrary string matching for use
// with tests.
class MockTabMatcher : public TabMatcher {
 public:
  MOCK_CONST_METHOD2(IsTabOpenWithURL,
                     bool(const GURL&, const AutocompleteInput*));
  MockTabMatcher();
  ~MockTabMatcher() override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOCK_TAB_MATCHER_H_
