// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider_client.h"

#include "base/notreached.h"

history_clusters::HistoryClustersService*
AutocompleteProviderClient::GetHistoryClustersService() {
  return nullptr;
}

history_embeddings::HistoryEmbeddingsService*
AutocompleteProviderClient::GetHistoryEmbeddingsService() {
  return nullptr;
}

DocumentSuggestionsService*
AutocompleteProviderClient::GetDocumentSuggestionsService() const {
  return nullptr;
}

bool AutocompleteProviderClient::AllowDeletingBrowserHistory() const {
  return true;
}

std::string AutocompleteProviderClient::ProfileUserName() const {
  return "";
}

bool AutocompleteProviderClient::IsIncognitoModeAvailable() const {
  return true;
}

bool AutocompleteProviderClient::IsSharingHubAvailable() const {
  return false;
}

bool AutocompleteProviderClient::IsHistoryEmbeddingsEnabled() const {
  return false;
}

bool AutocompleteProviderClient::IsHistoryEmbeddingsSettingVisible() const {
  return false;
}

bool AutocompleteProviderClient::IsLensEnabled() const {
  return false;
}

bool AutocompleteProviderClient::AreLensEntrypointsVisible() const {
  return false;
}

std::optional<bool> AutocompleteProviderClient::IsPagePaywalled() const {
  return std::nullopt;
}

bool AutocompleteProviderClient::in_background_state() const {
  return false;
}

base::WeakPtr<AutocompleteProviderClient>
AutocompleteProviderClient::GetWeakPtr() {
  return nullptr;
}
