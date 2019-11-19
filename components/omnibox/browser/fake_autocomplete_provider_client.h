// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
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
class ShortcutsBackend;

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

  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  history::HistoryService* GetHistoryService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  InMemoryURLIndex* GetInMemoryURLIndex() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackend() override;
  scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() override;

  void set_in_memory_url_index(std::unique_ptr<InMemoryURLIndex> index) {
    in_memory_url_index_ = std::move(index);
  }

  bool IsTabOpenWithURL(const GURL& url,
                        const AutocompleteInput* input) override;

  // A test calls this to establish the set of URLs that will return
  // true from IsTabOpenWithURL() above. It's a simple substring match
  // of the URL.
  void set_url_substring_match(const std::string& substr) {
    substring_to_match_ = substr;
  }

 private:
  base::ScopedTempDir history_dir_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  TestSchemeClassifier scheme_classifier_;
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index_;
  std::unique_ptr<history::HistoryService> history_service_;
  scoped_refptr<ShortcutsBackend> shortcuts_backend_;

  // Substring used to match URLs for IsTabOpenWithURL().
  std::string substring_to_match_;

  DISALLOW_COPY_AND_ASSIGN(FakeAutocompleteProviderClient);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_PROVIDER_CLIENT_H_
