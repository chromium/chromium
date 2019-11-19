// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/mock_autocomplete_provider_client.h"

#include <memory>

MockAutocompleteProviderClient::MockAutocompleteProviderClient() {
  shared_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);

  remote_suggestions_service_ =
      std::make_unique<RemoteSuggestionsService>(GetURLLoaderFactory());
  document_suggestions_service_ = std::make_unique<DocumentSuggestionsService>(
      /*identity_manager=*/nullptr, GetURLLoaderFactory());
  pedal_provider_ = std::make_unique<OmniboxPedalProvider>(*this);
  browser_update_available_ = false;
}

MockAutocompleteProviderClient::~MockAutocompleteProviderClient() {
}
