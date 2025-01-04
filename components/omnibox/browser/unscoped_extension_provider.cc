// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/unscoped_extension_provider.h"

#include <string>

#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/unscoped_extension_provider_delegate.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

UnscopedExtensionProvider::UnscopedExtensionProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION),
      client_(client),
      template_url_service_(client->GetTemplateURLService()),
      delegate_(client->GetUnscopedExtensionProviderDelegate(this)) {
  AddListener(listener);
}

UnscopedExtensionProvider::~UnscopedExtensionProvider() = default;

void UnscopedExtensionProvider::Start(const AutocompleteInput& input,
                                      bool minimal_changes) {
  // No need to do other checks if there are no unscoped extensions.
  std::set<std::string> unscoped_extensions = GetUnscopedModeExtensionIds();
  if (unscoped_extensions.empty()) {
    return;
  }

  // TODO(378538411): investigate enabling zero-suggest to the extensions.
  if (input.IsZeroSuggest()) {
    return;
  }

  // Return early if only synchronous matches are needed and the changes are not
  // minimal. Minimal changes will not need an async call.
  if (input.omit_asynchronous_matches() && !minimal_changes) {
    return;
  }

  // Do not send anything in keyword mode.
  // TODO(378538411): Figure out if `done_` needs to be reset.
  if (input.InKeywordMode()) {
    return;
  }

  if (!minimal_changes) {
    // Reset done and increment the input ID to discard any stale extension
    // suggestions that may be incoming later if the current request id and
    // incoming request ids do not match.
    done_ = true;
    delegate_->IncrementRequestId();
  }
  delegate_->Start(input, minimal_changes, unscoped_extensions);
}

void UnscopedExtensionProvider::Stop(bool clear_cached_results,
                                     bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
  delegate_->IncrementRequestId();
}

std::set<std::string> UnscopedExtensionProvider::GetUnscopedModeExtensionIds()
    const {
  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  template_url_service_->Load();
  return template_url_service_->GetUnscopedModeExtensionIds();
}
