// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"
#include "base/metrics/field_trial_params.h"

#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ntp_snippets {

namespace {

// Variation parameter for chrome-content-suggestions backend.
const char kContentSuggestionsBackend[] = "content_suggestions_backend";

}  // namespace

GURL GetFetchEndpoint() {
  std::string endpoint = base::GetFieldTrialParamValueByFeature(
      ntp_snippets::kArticleSuggestionsFeature, kContentSuggestionsBackend);
  if (!endpoint.empty()) {
    return GURL{endpoint};
  }
  return GURL{kContentSuggestionsServer};
}

RemoteSuggestionsFetcher::~RemoteSuggestionsFetcher() = default;

}  // namespace ntp_snippets
