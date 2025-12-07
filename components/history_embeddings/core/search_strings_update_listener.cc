// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/core/search_strings_update_listener.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace history_embeddings {

namespace {

// Reads the contents of `file_path` and returns an instance of the
// HistorySearchStrings proto if the file contents can be parsed into it.
// Returns nullptr otherwise.
std::unique_ptr<optimization_guide::proto::HistorySearchStrings>
LoadSearchStringsFile(const base::FilePath& file_path) {
  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    DVLOG(1) << "Failed to read: " << file_path.LossyDisplayName();
    return nullptr;
  }

  auto proto =
      std::make_unique<optimization_guide::proto::HistorySearchStrings>();
  if (!proto->ParseFromString(file_content)) {
    DVLOG(1) << "Failed to parse contents of: " << file_path.LossyDisplayName();
    return nullptr;
  }

  DVLOG(1) << "Successfully parsed contents of: "
           << file_path.LossyDisplayName();
  return proto;
}

}  // namespace

// static
SearchStringsUpdateListener* SearchStringsUpdateListener::GetInstance() {
  static base::NoDestructor<SearchStringsUpdateListener> listener;
  return listener.get();
}

void SearchStringsUpdateListener::OnSearchStringsUpdate(
    const base::FilePath& file_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&LoadSearchStringsFile, file_path),
      base::BindOnce(&SearchStringsUpdateListener::SetSearchStrings,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchStringsUpdateListener::ResetForTesting() {
  filter_words_hashes_.clear();
  stop_words_hashes_.clear();
}

void SearchStringsUpdateListener::SetSearchStrings(
    std::unique_ptr<optimization_guide::proto::HistorySearchStrings> strings) {
  if (!strings) {
    return;
  }

  filter_words_hashes_.clear();
  for (std::string_view hash_string : strings->filter_words()) {
    uint32_t hash;
    if (base::StringToUint(hash_string, &hash)) {
      filter_words_hashes_.insert(hash);
    }
  }

  stop_words_hashes_.clear();
  for (std::string_view hash_string : strings->stop_words()) {
    uint32_t hash;
    if (base::StringToUint(hash_string, &hash)) {
      stop_words_hashes_.insert(hash);
    }
  }
}

SearchStringsUpdateListener::SearchStringsUpdateListener() = default;
SearchStringsUpdateListener::~SearchStringsUpdateListener() = default;

}  // namespace history_embeddings
