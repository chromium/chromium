// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TAB_GROUP_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_TAB_GROUP_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"

// This provider matches user input against tab groups. It is *not* included as
// a default provider.
class TabGroupProvider : public AutocompleteProvider {
 public:
  explicit TabGroupProvider(AutocompleteProviderClient* client);

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~TabGroupProvider() override;

  AutocompleteMatch CreateTabGroupMatch(const AutocompleteInput& input,
                                        const tab_groups::SavedTabGroup& group,
                                        int score);

  raw_ptr<AutocompleteProviderClient> client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TAB_GROUP_PROVIDER_H_
