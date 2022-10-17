// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/file_search_provider.h"

#include <cctype>
#include <cmath>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/input_method/diacritics_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"

namespace app_list {

namespace {

using ::ash::input_method::HasDiacritics;
using ::ash::string_matching::TokenizedString;

constexpr char kFileSearchSchema[] = "file_search://";
constexpr int kMaxResults = 25;
constexpr int kSearchTimeoutMs = 100;

// Construct a case-insensitive and accent-insensitive fnmatch query from
// |query|. E.g. for abc123, the result would be *[aAáàâäāåÁÀÂÄĀÅ][bB][cC]123*.
// Accent-insensitivity covers Latin-script accented characters for our
// initial implementation.
// We still honor the accented characters in |query|, and only enable
// case-insensitivity for them. E.g. ádd, the result would be *[áÁ]dd*.
std::string CreateFnmatchQuery(const std::u16string& query_input) {
  static constexpr auto conversion_map =
      base::MakeFixedFlatMap<char16_t, std::u16string_view>({
          {u'a', u"[aAáàâäāåÁÀÂÄĀÅ]"},
          {u'c', u"[cçCÇ]"},
          {u'e', u"[eEéèêëēÉÈÊËĒ]"},
          {u'i', u"[iIíìîïīÍÌÎÏĪ]"},
          {u'n', u"[nNñÑ]"},
          {u'o', u"[oOóòôöōøÓÒÔÖŌØ]"},
          {u'u', u"[uUúùûüūÚÙÛÜŪ]"},
          {u'y', u"[yYýỳŷÿȳÝỲŶŸȲ]"},
      });

  std::vector<std::u16string> query_pieces = {u"*"};
  size_t sequence_start = 0;
  const std::u16string query = base::i18n::ToLower(query_input);
  for (size_t i = 0; i < query.size(); ++i) {
    if ((query[i] >= u'a' && query[i] <= u'z') ||
        HasDiacritics(query.substr(i, 1))) {
      if (sequence_start != i) {
        query_pieces.push_back(
            query.substr(sequence_start, i - sequence_start));
      }

      auto* it = conversion_map.find(query[i]);
      if (it != conversion_map.end()) {
        std::u16string piece(it->second);
        query_pieces.push_back(std::move(piece));
      } else {
        query_pieces.push_back(u"[");
        query_pieces.push_back(query.substr(i, 1));
        query_pieces.push_back(base::i18n::ToUpper(query.substr(i, 1)));
        query_pieces.push_back(u"]");
      }

      sequence_start = i + 1;
    }
  }
  if (sequence_start != query.size()) {
    query_pieces.push_back(query.substr(sequence_start));
  }
  query_pieces.push_back(u"*");

  return base::UTF16ToUTF8(base::StrCat(query_pieces));
}

// Returns a vector of matched filepaths and a bool indicating whether or not
// the path is a directory.
std::vector<FileSearchProvider::FileInfo> SearchFilesByPattern(
    const base::FilePath& root_path,
    const std::u16string& query,
    const base::TimeTicks& query_start_time) {
  base::FileEnumerator enumerator(
      root_path,
      /*recursive=*/true,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES,
      CreateFnmatchQuery(query), base::FileEnumerator::FolderSearchPolicy::ALL);

  const auto time_limit = base::Milliseconds(kSearchTimeoutMs);
  bool timed_out = false;
  std::vector<FileSearchProvider::FileInfo> matched_paths;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    matched_paths.emplace_back(
        path, enumerator.GetInfo().IsDirectory(),
        base::Time::FromTimeT(enumerator.GetInfo().stat().st_atime));

    if (matched_paths.size() == kMaxResults) {
      break;
    } else if (base::TimeTicks::Now() - query_start_time > time_limit) {
      timed_out = true;
      break;
    }
  }
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.FileSearchProvider.TimedOut", timed_out);
  return matched_paths;
}

}  // namespace

FileSearchProvider::FileSearchProvider(Profile* profile)
    : profile_(profile),
      thumbnail_loader_(profile),
      root_path_(file_manager::util::GetMyFilesFolderForProfile(profile)) {
  DCHECK(profile_);
  DCHECK(!root_path_.empty());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FileSearchProvider::~FileSearchProvider() = default;

ash::AppListSearchResultType FileSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kFileSearch;
}

void FileSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  query_start_time_ = base::TimeTicks::Now();

  // Clear results and cancel any outgoing requests.
  ClearResultsSilently();
  weak_factory_.InvalidateWeakPtrs();

  last_query_ = query;
  last_tokenized_query_.emplace(query, TokenizedString::Mode::kWords);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(SearchFilesByPattern, root_path_, query,
                     query_start_time_),
      base::BindOnce(&FileSearchProvider::OnSearchComplete,
                     weak_factory_.GetWeakPtr()));
}

void FileSearchProvider::OnSearchComplete(
    std::vector<FileSearchProvider::FileInfo> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SearchProvider::Results results;
  for (const auto& path : paths) {
    double relevance = FileResult::CalculateRelevance(
        last_tokenized_query_, path.path, path.last_accessed);
    DCHECK((relevance >= 0.0) && (relevance <= 1.0));
    results.push_back(MakeResult(path, relevance));
  }

  SwapResults(&results);
  UMA_HISTOGRAM_TIMES("Apps.AppList.FileSearchProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> FileSearchProvider::MakeResult(
    const FileSearchProvider::FileInfo& path,
    const double relevance) {
  const auto type = path.is_directory ? FileResult::Type::kDirectory
                                      : FileResult::Type::kFile;
  // Use the parent directory name as details text. Take care to remove newlines
  // and handle RTL as this is displayed directly.
  std::u16string parent_dir_name = base::CollapseWhitespace(
      path.path.DirName().BaseName().LossyDisplayName(), true);
  base::i18n::SanitizeUserSuppliedString(&parent_dir_name);

  auto result = std::make_unique<FileResult>(
      kFileSearchSchema, path.path, parent_dir_name,
      ash::AppListSearchResultType::kFileSearch,
      ash::SearchResultDisplayType::kList, relevance, last_query_, type,
      profile_);
  result->RequestThumbnail(&thumbnail_loader_);
  return result;
}

}  // namespace app_list
