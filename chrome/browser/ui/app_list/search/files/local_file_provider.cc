// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/local_file_provider.h"

#include <cctype>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;

constexpr char kLocalFileSchema[] = "local_file://";
constexpr int kMaxResults = 25;

// Construct a case-insensitive fnmatch query from |query|. E.g. for abc123, the
// result would be *[aA][bB][cC]123*.
std::string CreateFnmatchQuery(const std::string& query) {
  std::vector<std::string> query_pieces = {"*"};
  size_t sequence_start = 0;
  for (size_t i = 0; i < query.size(); ++i) {
    if (isalpha(query[i])) {
      if (sequence_start != i) {
        query_pieces.push_back(
            query.substr(sequence_start, i - sequence_start));
      }
      std::string piece("[");
      piece.resize(4);
      piece[1] = tolower(query[i]);
      piece[2] = toupper(query[i]);
      piece[3] = ']';
      query_pieces.push_back(std::move(piece));
      sequence_start = i + 1;
    }
  }
  if (sequence_start != query.size()) {
    query_pieces.push_back(query.substr(sequence_start));
  }
  query_pieces.push_back("*");

  return base::StrCat(query_pieces);
}

}  // namespace

LocalFileProvider::LocalFileProvider(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

LocalFileProvider::~LocalFileProvider() = default;

ash::AppListSearchResultType LocalFileProvider::ResultType() {
  return ash::AppListSearchResultType::kLocalFile;
}

void LocalFileProvider::Start(const std::u16string& query) {
  // Clear results and cancel any outgoing requests.
  ClearResultsSilently();
  weak_factory_.InvalidateWeakPtrs();

  // This provider does not handle zero-state.
  if (query.empty())
    return;

  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LocalFileProvider::SearchFilesByPattern,
                     weak_factory_.GetWeakPtr(), base::UTF16ToUTF8(query)));
}

void LocalFileProvider::SearchFilesByPattern(const std::string& query) {
  base::FileEnumerator enumerator(
      file_manager::util::GetMyFilesFolderForProfile(profile_),
      /*recursive=*/true, base::FileEnumerator::FILES,
      CreateFnmatchQuery(query), base::FileEnumerator::FolderSearchPolicy::ALL);
  SearchProvider::Results results;

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    results.emplace_back(MakeResult(path));

    // TODO(crbug.com/1154513): Also exit early if we reach a timeout.
    if (results.size() == kMaxResults)
      break;
  }
  SwapResults(&results);
  // TODO(crbug.com/1154513): Log success and latency histograms.
}

std::unique_ptr<FileResult> LocalFileProvider::MakeResult(
    const base::FilePath& path) {
  const double relevance =
      CalculateFilenameRelevance(last_tokenized_query_, path);
  return std::make_unique<FileResult>(
      kLocalFileSchema, path, ash::AppListSearchResultType::kLocalFile,
      ash::SearchResultDisplayType::kList, relevance, profile_);
}

}  // namespace app_list
