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

FakeAutocompleteProviderClient::FakeAutocompleteProviderClient() {
  set_template_url_service(
      search_engines_test_enviroment_.template_url_service());

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  on_device_tail_model_service_ =
      std::make_unique<FakeOnDeviceTailModelService>();
  scoring_model_service_ =
      std::make_unique<FakeAutocompleteScoringModelService>();
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

FakeAutocompleteProviderClient::~FakeAutocompleteProviderClient() {
  // `ShortcutsBackend` depends on `TemplateURLService` so it should be
  // destroyed before it.
  shortcuts_backend_.reset();

  // We explicitly set `TemplateURLService` to `nullptr` because the parent
  // `MockAutocompleteProviderClient` class  has a pointer to
  // `TemplateURLService` which lives in the `SearchEnginesTestEnvironment`
  // object in this class.
  set_template_url_service(nullptr);
  // The InMemoryURLIndex must be explicitly shut down or it will DCHECK() in
  // its destructor.
  if (in_memory_url_index_)
    in_memory_url_index_->Shutdown();
  if (history_service_)
    history_service_->Shutdown();
}

PrefService* FakeAutocompleteProviderClient::GetPrefs() const {
  return &search_engines_test_enviroment_.pref_service();
}

PrefService* FakeAutocompleteProviderClient::GetLocalState() {
  return &search_engines_test_enviroment_.local_state();
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

#if !BUILDFLAG(IS_IOS)
history_embeddings::HistoryEmbeddingsService*
FakeAutocompleteProviderClient::GetHistoryEmbeddingsService() {
  return history_embeddings_service_.get();
}
#endif

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

const TabMatcher& FakeAutocompleteProviderClient::GetTabMatcher() const {
  return fake_tab_matcher_;
}

scoped_refptr<history::TopSites> FakeAutocompleteProviderClient::GetTopSites() {
  return top_sites_;
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
OnDeviceTailModelService*
FakeAutocompleteProviderClient::GetOnDeviceTailModelService() const {
  return on_device_tail_model_service_.get();
}

FakeAutocompleteScoringModelService*
FakeAutocompleteProviderClient::GetAutocompleteScoringModelService() const {
  return scoring_model_service_.get();
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
