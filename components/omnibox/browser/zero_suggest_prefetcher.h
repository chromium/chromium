// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PREFETCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PREFETCHER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/omnibox/browser/autocomplete_input.h"

class AutocompleteController;
class AutocompleteProviderClient;

/**
 * A class responsible for prefetching zero-prefix suggestions using a temporary
 * AutocompleteController instance upon instantiation. Invokes StartPrefetch()
 * instead of Start() on AutocompleteController if |use_prefetch_path| is true.
 */
class ZeroSuggestPrefetcher {
 public:
  ZeroSuggestPrefetcher(std::unique_ptr<AutocompleteProviderClient> client,
                        AutocompleteInput input,
                        bool use_prefetch_path);
  ZeroSuggestPrefetcher(const ZeroSuggestPrefetcher&) = delete;
  ZeroSuggestPrefetcher& operator=(const ZeroSuggestPrefetcher&) = delete;

 private:
  ~ZeroSuggestPrefetcher();

  void SelfDestruct();

  std::unique_ptr<AutocompleteController> controller_;
  base::OneShotTimer expire_timer_;

  base::WeakPtrFactory<ZeroSuggestPrefetcher> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ZERO_SUGGEST_PREFETCHER_H_
