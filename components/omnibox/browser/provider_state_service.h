// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_PROVIDER_STATE_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_PROVIDER_STATE_SERVICE_H_

#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_match.h"

// Autocomplete providers' lifetimes are per browser window. This struct track
// provider state that needs to be shared between windows.
struct ProviderStateService : public KeyedService {
  struct CachedAutocompleteMatch {
    AutocompleteMatch match;
    base::TimeTicks time;
  };

  ProviderStateService();
  ~ProviderStateService() override;
  ProviderStateService(const ProviderStateService&) = delete;
  ProviderStateService& operator=(const ProviderStateService&) = delete;

  std::vector<CachedAutocompleteMatch> calculator_provider_cache;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_PROVIDER_STATE_SERVICE_H_
