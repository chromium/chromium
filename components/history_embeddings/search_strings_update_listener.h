// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_SEARCH_STRINGS_UPDATE_LISTENER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_SEARCH_STRINGS_UPDATE_LISTENER_H_

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/optimization_guide/proto/features/history_search_strings.pb.h"

namespace history_embeddings {

// Used by HistoryEmbeddingsService to hold the filter words hashes received via
// Finch. It will eventually be used by a ComponentInstaller to hold the
// filter words hashes received via Component Updater.
// TODO(b/365559465): Update this comment once ComponentInstaller is set up.
class SearchStringsUpdateListener {
 public:
  static SearchStringsUpdateListener* GetInstance();
  SearchStringsUpdateListener(const SearchStringsUpdateListener&) = delete;
  SearchStringsUpdateListener& operator=(const SearchStringsUpdateListener&) =
      delete;

  // Called by ComponentInstaller when the search strings file is installed.
  void OnSearchStringsUpdate(const base::FilePath& file_path);

  // Populates `filter_words_hashes_` from the comma-separated input string.
  // TODO(b/365559465): Remove this function once ComponentInstaller is set up.
  void SetFilterWordsHashes(const std::string_view filter_words_hashes);

  const std::unordered_set<uint32_t>& filter_words_hashes() {
    return filter_words_hashes_;
  }

  const std::unordered_set<uint32_t>& stop_words_hashes() {
    return stop_words_hashes_;
  }

  // Clear all hashes.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<SearchStringsUpdateListener>;

  void SetSearchStrings(
      std::unique_ptr<optimization_guide::proto::HistorySearchStrings> strings);

  SearchStringsUpdateListener();
  ~SearchStringsUpdateListener();

  // Hashes for phrases of one or two words to be filtered.
  std::unordered_set<uint32_t> filter_words_hashes_;

  // Hashes for stop words to be removed from query terms before text search.
  std::unordered_set<uint32_t> stop_words_hashes_;

  base::WeakPtrFactory<SearchStringsUpdateListener> weak_ptr_factory_{this};
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SEARCH_STRINGS_UPDATE_LISTENER_H_
