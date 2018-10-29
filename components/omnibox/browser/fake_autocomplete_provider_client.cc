// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fake_autocomplete_provider_client.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/in_memory_url_index_test_util.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"

FakeAutocompleteProviderClient::FakeAutocompleteProviderClient(
    bool create_history_db) {
  set_template_url_service(std::make_unique<TemplateURLService>(nullptr, 0));

  bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

  CHECK(history_dir_.CreateUniqueTempDir());
  history_service_ =
      history::CreateHistoryService(history_dir_.GetPath(), create_history_db);

  in_memory_url_index_.reset(
      new InMemoryURLIndex(bookmark_model_.get(), history_service_.get(),
                           nullptr, history_dir_.GetPath(), SchemeSet()));
  in_memory_url_index_->Init();

  shortcuts_backend_ = base::MakeRefCounted<ShortcutsBackend>(
      GetTemplateURLService(), std::make_unique<SearchTermsData>(),
      GetHistoryService(), base::FilePath(), true);
  shortcuts_backend_->Init();
}

FakeAutocompleteProviderClient::~FakeAutocompleteProviderClient() {
  // The InMemoryURLIndex must be explicitly shut down or it will DCHECK() in
  // its destructor.
  GetInMemoryURLIndex()->Shutdown();
  set_in_memory_url_index(nullptr);
  // History service can have its own thread. So we need to explicitly shut down
  // it to prevent memory leaks.
  GetHistoryService()->Shutdown();
  // Note that RunUntilIdle() must still be called after this, from
  // whichever task model is being used, probably ScopedTaskEnvironment,
  // or there will be memory leaks.
}

const AutocompleteSchemeClassifier&
FakeAutocompleteProviderClient::GetSchemeClassifier() const {
  return scheme_classifier_;
}

history::HistoryService* FakeAutocompleteProviderClient::GetHistoryService() {
  return history_service_.get();
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

bool FakeAutocompleteProviderClient::IsTabOpenWithURL(
    const GURL& url,
    const AutocompleteInput* input) {
  return !substring_to_match_.empty() &&
         url.spec().find(substring_to_match_) != std::string::npos;
}
