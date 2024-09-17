// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/omnibox/browser/fake_tab_matcher.h"

FakeTabMatcher::FakeTabMatcher() = default;
FakeTabMatcher::~FakeTabMatcher() = default;

void FakeTabMatcher::AddOpenTab(TabMatcher::TabWrapper open_tab) {
  open_tabs_.push_back(open_tab);
}

bool FakeTabMatcher::IsTabOpenWithURL(const GURL& url,
                                      const AutocompleteInput* input) const {
  return !substring_to_match_.empty() &&
         url.spec().find(substring_to_match_) != std::string::npos;
}

std::vector<TabMatcher::TabWrapper> FakeTabMatcher::GetOpenTabs() const {
  return open_tabs_;
}
