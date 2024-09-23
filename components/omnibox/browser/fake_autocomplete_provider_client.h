// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/fake_tab_matcher.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search_engines/search_engines_test_environment.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/omnibox/browser/fake_autocomplete_scoring_model_service.h"
#include "components/omnibox/browser/fake_on_device_tail_model_service.h"
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

#if !BUILDFLAG(IS_IOS)
#include "components/history_embeddings/history_embeddings_service.h"  // nogncheck
#endif  // IS_IOS

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

namespace history_clusters {
class HistoryClustersService;
}

class InMemoryURLIndex;
class PrefService;

// Fully operational AutocompleteProviderClient for usage in tests.
// Note: The history index rebuild task is created from main thread, usually
// during SetUp(), performed on DB thread and must be deleted on main thread.
// Run main loop to process delete task, to prevent leaks.
// Note that these tests have switched to using a TaskEnvironment,
// so clearing that task queue is done through
// task_environment_.RunUntilIdle().
class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient();
  ~FakeAutocompleteProviderClient() override;
  FakeAutocompleteProviderClient(const FakeAutocompleteProviderClient&) =
      delete;
  FakeAutocompleteProviderClient& operator=(
      const FakeAutocompleteProviderClient&) = delete;

  PrefService* GetPrefs() const override;
  // Note: this will not be shared with other test fakes that may create their
  // own local_state testing PrefService.
  // In this case, AutocompleteProviderClient could be modified to accept the
  // local pref store in its constructor.
  PrefService* GetLocalState() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  history::HistoryService* GetHistoryService() override;
  history_clusters::HistoryClustersService* GetHistoryClustersService()
      override;
#if !BUILDFLAG(IS_IOS)
  history_embeddings::HistoryEmbeddingsService* GetHistoryEmbeddingsService()
      override;
#endif
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  InMemoryURLIndex* GetInMemoryURLIndex() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override;
  const TabMatcher& GetTabMatcher() const override;
  scoped_refptr<history::TopSites> GetTopSites() override;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  OnDeviceTailModelService* GetOnDeviceTailModelService() const override;
  FakeAutocompleteScoringModelService* GetAutocompleteScoringModelService()
      const override;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  // Test-only setters
  void set_bookmark_model(std::unique_ptr<bookmarks::BookmarkModel> model) {
    bookmark_model_ = std::move(model);
  }

  void set_history_service(std::unique_ptr<history::HistoryService> service) {
    history_service_ = std::move(service);
  }

  void set_history_clusters_service(
      history_clusters::HistoryClustersService* service) {
    history_clusters_service_ = service;
  }

#if !BUILDFLAG(IS_IOS)
  void set_history_embeddings_service(
      std::unique_ptr<history_embeddings::HistoryEmbeddingsService> service) {
    history_embeddings_service_ = std::move(service);
  }
#endif

  // There should be no reason to set this unless the tested provider actually
  // uses the AutocompleteProviderClient's InMemoryURLIndex, like the
  // HistoryQuickProvider does.
  void set_in_memory_url_index(std::unique_ptr<InMemoryURLIndex> index) {
    in_memory_url_index_ = std::move(index);
  }

  void set_top_sites(scoped_refptr<history::TopSites> top_sites) {
    top_sites_ = std::move(top_sites);
  }

  void set_shortcuts_backend(scoped_refptr<ShortcutsBackend> backend) {
    shortcuts_backend_ = std::move(backend);
  }

 private:
  search_engines::SearchEnginesTestEnvironment search_engines_test_enviroment_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  TestSchemeClassifier scheme_classifier_;
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index_;
  std::unique_ptr<history::HistoryService> history_service_;
  raw_ptr<history_clusters::HistoryClustersService> history_clusters_service_ =
      nullptr;
#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<history_embeddings::HistoryEmbeddingsService>
      history_embeddings_service_;
#endif
  scoped_refptr<ShortcutsBackend> shortcuts_backend_;
  FakeTabMatcher fake_tab_matcher_;
  scoped_refptr<history::TopSites> top_sites_;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::unique_ptr<FakeOnDeviceTailModelService> on_device_tail_model_service_;
  std::unique_ptr<FakeAutocompleteScoringModelService> scoring_model_service_;
#endif
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
