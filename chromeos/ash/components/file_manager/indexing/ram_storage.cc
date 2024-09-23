// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/ram_storage.h"

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>

#include "base/logging.h"
#include "chromeos/ash/components/file_manager/indexing/match.h"

namespace ash::file_manager {

RamStorage::RamStorage() = default;

RamStorage::~RamStorage() = default;

const std::set<int64_t> RamStorage::GetTermIdsForUrl(int64_t url_id) const {
  auto url_term_it = plain_index_.find(url_id);
  if (url_term_it == plain_index_.end()) {
    return empty_id_set_;
  }
  return url_term_it->second;
}

const std::set<int64_t> RamStorage::GetUrlIdsForTermId(int64_t term_id) const {
  auto term_match = posting_lists_.find(term_id);
  if (term_match == posting_lists_.end()) {
    return empty_id_set_;
  }
  return term_match->second;
}

size_t RamStorage::AddToPostingList(int64_t term_id, int64_t url_id) {
  auto it = posting_lists_.find(term_id);
  size_t count;
  if (it == posting_lists_.end()) {
    posting_lists_.emplace(std::piecewise_construct,
                           std::forward_as_tuple(term_id),
                           std::forward_as_tuple(std::set<int64_t>{url_id}));
    count = 1;
  } else {
    auto ret = it->second.emplace(url_id);
    count = ret.second ? 1 : 0;
  }
  AddToPlainIndex(url_id, term_id);
  return count;
}

size_t RamStorage::DeleteFromPostingList(int64_t term_id, int64_t url_id) {
  auto it = posting_lists_.find(term_id);
  size_t count;
  if (it == posting_lists_.end()) {
    count = 0;
  } else {
    count = it->second.erase(url_id);
  }
  DeleteFromPlainIndex(url_id, term_id);
  return count;
}

int64_t RamStorage::GetTokenId(const std::string& token_bytes) const {
  auto it = token_map_.find(token_bytes);
  if (it != token_map_.end()) {
    return it->second;
  }
  return -1;
}

int64_t RamStorage::GetOrCreateTokenId(const std::string& token_bytes) {
  int64_t token_id = GetTokenId(token_bytes);
  if (token_id >= 0) {
    return token_id;
  }
  const int64_t this_token_id = ++token_id_;
  token_map_.emplace(std::make_pair(token_bytes, this_token_id));
  return this_token_id;
}

int64_t RamStorage::GetTermId(const Term& term) const {
  int64_t token_id = GetTokenId(term.token_bytes());
  if (token_id == -1) {
    return -1;
  }
  std::tuple<std::string, int64_t> term_tuple{term.field(), token_id};
  auto term_it = term_map_.find(term_tuple);
  if (term_it == term_map_.end()) {
    return -1;
  }
  return term_it->second;
}

int64_t RamStorage::GetOrCreateTermId(const Term& term) {
  int64_t term_id = GetTermId(term);
  if (term_id >= 0) {
    return term_id;
  }
  int64_t this_term_id = ++term_id_;
  int64_t token_id = GetOrCreateTokenId(term.token_bytes());
  term_map_.emplace(
      std::make_pair(std::make_tuple(term.field(), token_id), this_term_id));
  return this_term_id;
}

int64_t RamStorage::GetUrlId(const GURL& url) const {
  auto it = url_to_id_.find(url);
  return (it != url_to_id_.end()) ? it->second : -1;
}

int64_t RamStorage::GetOrCreateUrlId(const GURL& url) {
  int64_t url_id = GetUrlId(url);
  if (url_id >= 0) {
    return url_id;
  }
  int64_t this_url_id = ++url_id_;
  url_to_id_.emplace(std::make_pair(url, this_url_id));
  return this_url_id;
}

int64_t RamStorage::MoveUrl(const GURL& from, const GURL& to) {
  auto it = url_to_id_.find(from);
  if (it == url_to_id_.end()) {
    return -1;
  }
  const int64_t url_id = it->second;
  url_to_id_.erase(it);
  url_to_id_.emplace(std::make_pair(to, url_id));
  // In RAM we store the whole file info object. Thus we have two places
  // where the URL may be stored; if we have the corresponding file info
  // correct its URL, too.
  auto file_info_it = url_id_to_file_info_.find(url_id);
  if (file_info_it != url_id_to_file_info_.end()) {
    file_info_it->second.file_url = to;
  }
  return url_id;
}

int64_t RamStorage::DeleteUrl(const GURL& url) {
  auto it = url_to_id_.find(url);
  if (it == url_to_id_.end()) {
    return -1;
  }
  const int64_t url_id = it->second;
  url_to_id_.erase(it);
  return url_id;
}

int64_t RamStorage::PutFileInfo(const FileInfo& file_info) {
  int64_t url_id = GetOrCreateUrlId(file_info.file_url);
  if (url_id == -1) {
    return -1;
  }
  auto it = url_id_to_file_info_.find(url_id);
  if (it == url_id_to_file_info_.end()) {
    url_id_to_file_info_.emplace(std::make_pair(url_id, file_info));
  } else {
    it->second = file_info;
  }
  return url_id;
}

std::optional<FileInfo> RamStorage::GetFileInfo(int64_t url_id) const {
  auto file_info_it = url_id_to_file_info_.find(url_id);
  if (file_info_it == url_id_to_file_info_.end()) {
    return std::nullopt;
  }
  return file_info_it->second;
}

int64_t RamStorage::DeleteFileInfo(int64_t url_id) {
  auto file_info_it = url_id_to_file_info_.find(url_id);
  if (file_info_it == url_id_to_file_info_.end()) {
    return url_id;
  }
  url_id_to_file_info_.erase(file_info_it);
  return url_id;
}

size_t RamStorage::AddTermIdsForUrl(const std::set<int64_t>& term_ids,
                                    int64_t url_id) {
  size_t added_terms_count = 0;
  for (const int64_t term_id : term_ids) {
    added_terms_count += AddToPostingList(term_id, url_id);
  }
  return added_terms_count;
}

size_t RamStorage::DeleteTermIdsForUrl(const std::set<int64_t>& term_ids,
                                       int64_t url_id) {
  size_t deleted_terms_count = 0;
  for (const int64_t term_id : term_ids) {
    deleted_terms_count += DeleteFromPostingList(term_id, url_id);
  }
  return deleted_terms_count;
}

void RamStorage::AddToPlainIndex(int64_t url_id, int64_t term_id) {
  auto it = plain_index_.find(url_id);
  if (it == plain_index_.end()) {
    plain_index_.emplace(std::piecewise_construct,
                         std::forward_as_tuple(url_id),
                         std::forward_as_tuple(std::set<int64_t>{term_id}));
  } else {
    it->second.emplace(term_id);
  }
}

void RamStorage::DeleteFromPlainIndex(int64_t url_id, int64_t term_id) {
  auto it = plain_index_.find(url_id);
  if (it != plain_index_.end()) {
    it->second.erase(term_id);
  }
}

}  // namespace ash::file_manager
