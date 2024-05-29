// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_index.h"

#include "base/time/time.h"

namespace ash::file_manager {

FileIndex::FileIndex(std::unique_ptr<IndexStorage> storage)
    : storage_(std::move(storage)) {}
FileIndex::~FileIndex() = default;

OpResults FileIndex::Init() {
  return storage_->Init() ? OpResults::kSuccess : OpResults::kUninitialized;
}

OpResults FileIndex::PutFileInfo(const FileInfo& file_info) {
  return storage_->PutFileInfo(file_info) == -1 ? OpResults::kGenericError
                                                : OpResults::kSuccess;
}

OpResults FileIndex::SetTerms(const std::vector<Term>& terms, const GURL& url) {
  if (terms.empty()) {
    return OpResults::kArgumentError;
  }
  // Arrange terms by field and remove duplicates and convert to internal IDs.
  int64_t url_id = storage_->GetUrlId(url);
  if (url_id == -1) {
    return OpResults::kFileMissing;
  }
  std::set<int64_t> term_id_set = ConvertToTermIds(terms);

  // If the given url_id already had some terms associated with it, remove terms
  // not specified in terms vector. Say, if url_id had terms {t1, t3, t8}
  // associated with it, and terms was {t1, t2}, we would compute {t3, t8} as
  // the difference between two collections and remove those.
  std::set<int64_t> url_term_ids = storage_->GetTermIdsForUrl(url_id);
  if (!url_term_ids.empty()) {
    std::set<int64_t> to_remove_terms;
    std::set_difference(
        url_term_ids.begin(), url_term_ids.end(), term_id_set.begin(),
        term_id_set.end(),
        std::inserter(to_remove_terms, to_remove_terms.begin()));
    storage_->DeleteTermIdsForUrl(to_remove_terms, url_id);
  }
  storage_->AddTermIdsForUrl(term_id_set, url_id);
  return OpResults::kSuccess;
}

OpResults FileIndex::MoveFile(const GURL& old_url, const GURL& new_url) {
  DCHECK(old_url.is_valid());
  DCHECK(new_url.is_valid());
  // Check for no-op.
  if (old_url == new_url) {
    return OpResults::kSuccess;
  }
  // Phase 1: Run some diagnostics; not strictly necessary but it gives more
  // accurate error reporting.
  int64_t old_url_id = storage_->GetUrlId(old_url);
  if (old_url_id < 0) {
    return OpResults::kFileMissing;
  }
  int64_t new_url_id = storage_->GetUrlId(new_url);
  if (new_url_id != -1) {
    return OpResults::kFileExists;
  }
  std::optional<FileInfo> file_info = storage_->GetFileInfo(old_url_id);
  if (!file_info.has_value()) {
    return OpResults::kFileMissing;
  }

  // Phase 2: Just make the move by updating URL.
  return storage_->MoveUrl(old_url, new_url) == -1 ? OpResults::kGenericError
                                                   : OpResults::kSuccess;
}

OpResults FileIndex::RemoveFile(const GURL& url) {
  int64_t url_id = storage_->GetUrlId(url);
  if (url_id < 0) {
    return OpResults::kSuccess;
  }
  const std::set<int64_t>& url_term_ids = storage_->GetTermIdsForUrl(url_id);
  for (int64_t term_id : url_term_ids) {
    storage_->DeleteFromPostingList(term_id, url_id);
  }
  storage_->DeleteFileInfo(url_id);
  storage_->DeleteUrl(url);
  return OpResults::kSuccess;
}

OpResults FileIndex::RemoveTerms(const std::vector<Term>& terms,
                                 const GURL& url) {
  int64_t url_id = storage_->GetUrlId(url);
  if (url_id < 0) {
    return OpResults::kSuccess;
  }
  std::set<int64_t> term_ids;
  for (const Term& t : terms) {
    int64_t id_with_field = storage_->GetTermId(t);
    if (id_with_field != -1) {
      term_ids.emplace(id_with_field);
    }
    int64_t global_id = storage_->GetTermId(Term("", t.token()));
    if (global_id != -1) {
      term_ids.emplace(global_id);
    }
  }
  for (int64_t term_id : term_ids) {
    storage_->DeleteFromPostingList(term_id, url_id);
  }
  return OpResults::kSuccess;
}

OpResults FileIndex::AddTerms(const std::vector<Term>& terms, const GURL& url) {
  if (terms.empty()) {
    return OpResults::kSuccess;
  }

  int64_t url_id = storage_->GetUrlId(url);
  if (url_id == -1) {
    return OpResults::kFileMissing;
  }

  std::set<int64_t> term_id_set = ConvertToTermIds(terms);
  storage_->AddTermIdsForUrl(term_id_set, url_id);
  return OpResults::kSuccess;
}

// Searches the index for file info matching the specified query.
SearchResults FileIndex::Search(const Query& query) {
  const std::vector<Term>& terms = query.terms();
  SearchResults results;
  if (terms.empty()) {
    // Technically, an empty query matches every file, but we treat this
    // as empty match.
    return results;
  }
  std::set<int64_t> matched_url_ids;
  bool first = true;
  for (const Term& term : terms) {
    int64_t term_id = storage_->GetTermId(term);
    if (term_id == -1) {
      return results;
    }
    const std::set<int64_t> url_ids = storage_->GetUrlIdsForTermId(term_id);
    if (url_ids.empty()) {
      return results;
    }
    if (first) {
      matched_url_ids = url_ids;
      first = false;
    } else {
      std::set<int64_t> intersection;
      std::set_intersection(matched_url_ids.begin(), matched_url_ids.end(),
                            url_ids.begin(), url_ids.end(),
                            std::inserter(intersection, intersection.begin()));
      matched_url_ids = intersection;
    }
    if (matched_url_ids.empty()) {
      break;
    }
  }
  if (matched_url_ids.empty()) {
    return results;
  }
  for (const int64_t url_id : matched_url_ids) {
    std::optional<FileInfo> file_info = storage_->GetFileInfo(url_id);
    DCHECK(file_info.has_value());
    // TODO(b:327535200): Add true score.
    results.matches.emplace_back(Match(1, file_info.value()));
  }
  // TODO(b:327535200): Correctly compute total_matches.
  results.total_matches = results.matches.size();
  return results;
}

std::set<int64_t> FileIndex::ConvertToTermIds(const std::vector<Term>& terms) {
  std::set<int64_t> term_ids;
  for (const Term& term : terms) {
    DCHECK(!term.field().empty());
    term_ids.emplace(storage_->GetOrCreateTermId(term));
    term_ids.emplace(storage_->GetOrCreateTermId(Term("", term.token())));
  }
  return term_ids;
}

}  // namespace ash::file_manager
