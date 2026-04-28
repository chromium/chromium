// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/file_activity_provider.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/record_replay/core/browser/parsing_utils.h"

namespace record_replay {

namespace {

std::optional<std::string> ReadFileToStringAsync(const base::FilePath& path) {
  std::string content;
  if (base::ReadFileToString(path, &content)) {
    return content;
  }
  return std::nullopt;
}

}  // namespace

FileActivityProvider::FileActivityProvider(const base::FilePath& file_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileToStringAsync, file_path),
      base::BindOnce(&FileActivityProvider::OnFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

FileActivityProvider::~FileActivityProvider() = default;

std::optional<ActivityDiscoveryService::AutomationMetadata>
FileActivityProvider::GetMetadata(const GURL& url) const {
  auto it = metadata_map_.find(url);
  if (it != metadata_map_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void FileActivityProvider::ShouldOfferActivity(
    const GURL& url,
    base::OnceCallback<
        void(std::optional<ActivityDiscoveryService::AutomationMetadata>)>
        callback) {
  if (!is_ready_) {
    pending_queries_.emplace_back(url, std::move(callback));
    return;
  }

  std::move(callback).Run(GetMetadata(url));
}

void FileActivityProvider::OnFileLoaded(
    std::optional<std::string> file_content) {
  if (!file_content) {
    OnJsonParsed({});
    return;
  }

  std::vector<base::Value> values = ParseJSONListOfDicts(*file_content);
  OnJsonParsed(std::move(values));
}

void FileActivityProvider::OnJsonParsed(std::vector<base::Value> values) {
  is_ready_ = true;

  for (const auto& item : values) {
    if (!item.is_dict()) {
      continue;
    }
    const auto& dict = item.GetDict();
    const std::string* url_str = dict.FindString("url");
    const std::string* title = dict.FindString("title");
    const std::string* instructions = dict.FindString("instructions");
    const std::string* anchored_message = dict.FindString("anchored_message");

    if (url_str && title && instructions && anchored_message) {
      GURL url(*url_str);
      if (url.is_valid()) {
        metadata_map_.emplace(url, ActivityDiscoveryService::AutomationMetadata{
                                       .title = *title,
                                       .instructions = *instructions,
                                       .anchored_message = *anchored_message});
      }
    }
  }

  for (auto& query : pending_queries_) {
    std::move(query.second).Run(GetMetadata(query.first));
  }
  pending_queries_.clear();
}

}  // namespace record_replay
