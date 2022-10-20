// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fake_autocomplete_provider_client.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/prefs/testing_pref_service.h"
#include "components/query_tiles/test/fake_tile_service.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"

FakeAutocompleteProviderClient::FakeAutocompleteProviderClient() {
  set_template_url_service(std::make_unique<TemplateURLService>(nullptr, 0));

  pref_service_ = std::make_unique<TestingPrefServiceSimple>();
  local_state_ = std::make_unique<TestingPrefServiceSimple>();
  tile_service_ = std::make_unique<query_tiles::FakeTileService>();
}

FakeAutocompleteProviderClient::~FakeAutocompleteProviderClient() {
  // The InMemoryURLIndex must be explicitly shut down or it will DCHECK() in
  // its destructor.
  if (in_memory_url_index_)
    in_memory_url_index_->Shutdown();
  if (history_service_)
    history_service_->Shutdown();
}

PrefService* FakeAutocompleteProviderClient::GetPrefs() const {
  return pref_service_.get();
}

PrefService* FakeAutocompleteProviderClient::GetLocalState() {
  return local_state_.get();
}

const AutocompleteSchemeClassifier&
FakeAutocompleteProviderClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

history::HistoryService* FakeAutocompleteProviderClient::GetHistoryService() {
  return history_service_.get();
}

history_clusters::HistoryClustersService*
FakeAutocompleteProviderClient::GetHistoryClustersService() {
  return history_clusters_service_;
}

bookmarks::BookmarkModel* FakeAutocompleteProviderClient::GetBookmarkModel() {
  return bookmark_model_.get();
}

InMemoryURLIndex* FakeAutocompleteProviderClient::GetInMemoryURLIndex() {
  return in_memory_url_index_.get();
}

scoped_refptr<ShortcutsBackend>
FakeAutocompleteProviderClient::GetShortcutsBackend() {
  return shortcuts_backend_;
}

scoped_refptr<ShortcutsBackend>
FakeAutocompleteProviderClient::GetShortcutsBackendIfExists() {
  return shortcuts_backend_;
}

query_tiles::TileService* FakeAutocompleteProviderClient::GetQueryTileService()
    const {
  return tile_service_.get();
}

const TabMatcher& FakeAutocompleteProviderClient::GetTabMatcher() const {
  return fake_tab_matcher_;
}

scoped_refptr<history::TopSites> FakeAutocompleteProviderClient::GetTopSites() {
  return top_sites_;
}
