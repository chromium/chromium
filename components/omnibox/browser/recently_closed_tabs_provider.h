// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_RECENTLY_CLOSED_TABS_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_RECENTLY_CLOSED_TABS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteInput;
class AutocompleteProviderClient;
class AutocompleteProviderListener;

class RecentlyClosedTabsProvider : public AutocompleteProvider {
 public:
  RecentlyClosedTabsProvider(AutocompleteProviderClient* client,
                             AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

 private:
  ~RecentlyClosedTabsProvider() override;

  raw_ptr<AutocompleteProviderClient> client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_RECENTLY_CLOSED_TABS_PROVIDER_H_
