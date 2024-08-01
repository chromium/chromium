// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider_client.h"

history_clusters::HistoryClustersService*
AutocompleteProviderClient::GetHistoryClustersService() {
  return nullptr;
}

history_embeddings::HistoryEmbeddingsService*
AutocompleteProviderClient::GetHistoryEmbeddingsService() {
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

bool AutocompleteProviderClient::in_background_state() const {
  return false;
}

base::WeakPtr<AutocompleteProviderClient>
AutocompleteProviderClient::GetWeakPtr() {
  return nullptr;
}
