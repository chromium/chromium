// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/open_tab_provider.h"

#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/tab_matcher.h"

#if !BUILDFLAG(IS_IOS)
#include "content/public/browser/web_contents.h"
#endif  // !BUILDFLAG(IS_IOS)

OpenTabProvider::OpenTabProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_OPEN_TAB),
      client_(client) {}

OpenTabProvider::~OpenTabProvider() = default;

void OpenTabProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if (input.focus_type() != OmniboxFocusType::DEFAULT) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  for (auto* web_contents : client_->GetTabMatcher().GetOpenTabs()) {
    // TODO(crbug.com/1293702): This is a placeholder implementation for the WIP
    // matching and scoring algorithm.
    if (web_contents) {
      AutocompleteMatch empty_match;
      matches_.push_back(empty_match);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}
