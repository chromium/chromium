// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/mock_autocomplete_provider_client.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/omnibox/browser/document_suggestions_service.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/provider_state_service.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

MockAutocompleteProviderClient::MockAutocompleteProviderClient() {
  shared_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  document_suggestions_service_ = std::make_unique<DocumentSuggestionsService>(
      /*identity_manager=*/nullptr, GetURLLoaderFactory());
  remote_suggestions_service_ = std::make_unique<RemoteSuggestionsService>(
      document_suggestions_service_.get(), GetURLLoaderFactory());
  omnibox_triggered_feature_service_ =
      std::make_unique<OmniboxTriggeredFeatureService>();
  provider_state_service_ = std::make_unique<ProviderStateService>();
}

MockAutocompleteProviderClient::~MockAutocompleteProviderClient() = default;
