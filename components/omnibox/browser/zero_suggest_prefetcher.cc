// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_prefetcher.h"

#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"

ZeroSuggestPrefetcher::ZeroSuggestPrefetcher(
    std::unique_ptr<AutocompleteProviderClient> client,
    AutocompleteInput input,
    bool use_prefetch_path)
    : controller_(std::make_unique<AutocompleteController>(
          std::move(client),
          AutocompleteProvider::TYPE_ZERO_SUGGEST)) {
  if (use_prefetch_path) {
    controller_->StartPrefetch(input);
  } else {
    controller_->Start(input);
  }

  // Self-destruct after a duration given by
  // OmniboxFieldTrial::StopTimerFieldTrialDuration(). This should be enough
  // time to cache results or give up if the results haven't been received.
  expire_timer_.Start(FROM_HERE,
                      OmniboxFieldTrial::StopTimerFieldTrialDuration(),
                      base::BindOnce(&ZeroSuggestPrefetcher::SelfDestruct,
                                     weak_ptr_factory_.GetWeakPtr()));
}

ZeroSuggestPrefetcher::~ZeroSuggestPrefetcher() = default;

void ZeroSuggestPrefetcher::SelfDestruct() {
  delete this;
}
