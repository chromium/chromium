// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "components/omnibox/browser/fake_tab_matcher.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace history {
class HistoryService;
}  // namespace history

class InMemoryURLIndex;
class PrefService;
class ShortcutsBackend;
class TestingPrefServiceSimple;

// Fully operational AutocompleteProviderClient for usage in tests.
// Note: The history index rebuild task is created from main thread, usually
// during SetUp(), performed on DB thread and must be deleted on main thread.
// Run main loop to process delete task, to prevent leaks.
// Note that these tests have switched to using a TaskEnvironment,
// so clearing that task queue is done through
// task_environment_.RunUntilIdle().
class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  explicit FakeAutocompleteProviderClient(bool create_history_db = true);
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
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  InMemoryURLIndex* GetInMemoryURLIndex() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override;
  query_tiles::TileService* GetQueryTileService() const override;
  const TabMatcher& GetTabMatcher() const override;

  void set_in_memory_url_index(std::unique_ptr<InMemoryURLIndex> index) {
    in_memory_url_index_ = std::move(index);
  }

 private:
  base::ScopedTempDir history_dir_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  TestSchemeClassifier scheme_classifier_;
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<ShortcutsBackend> shortcuts_backend_;
  std::unique_ptr<query_tiles::TileService> tile_service_;
  FakeTabMatcher fake_tab_matcher_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
