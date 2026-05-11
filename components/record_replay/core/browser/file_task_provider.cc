// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/file_task_provider.h"

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/record_replay/core/browser/parsing_utils.h"
#include "components/record_replay/core/browser/task_definition_parsing_utils.h"

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

FileTaskProvider::FileTaskProvider(const base::FilePath& file_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileToStringAsync, file_path),
      base::BindOnce(&FileTaskProvider::OnFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

FileTaskProvider::~FileTaskProvider() = default;

std::optional<TaskDiscoveryService::AutomationMetadata>
FileTaskProvider::GetMetadata(const GURL& url) const {
  auto it = metadata_map_.find(url);
  if (it != metadata_map_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void FileTaskProvider::ShouldOfferTask(
    const GURL& url,
    base::OnceCallback<void(
        std::optional<TaskDiscoveryService::AutomationMetadata>)> callback) {
  if (!is_ready_) {
    pending_queries_.emplace_back(url, std::move(callback));
    return;
  }

  std::move(callback).Run(GetMetadata(url));
}

void FileTaskProvider::OnFileLoaded(std::optional<std::string> file_content) {
  if (!file_content) {
    OnJsonParsed({});
    return;
  }

  std::vector<base::Value> values = ParseJSONListOfDicts(*file_content);
  OnJsonParsed(std::move(values));
}

void FileTaskProvider::OnJsonParsed(std::vector<base::Value> values) {
  is_ready_ = true;

  for (const auto& item : values) {
    if (!item.is_dict()) {
      continue;
    }

    auto result = ParseTaskDefinition(item.GetDict());
    if (result.has_value()) {
      const TaskDefinition& task_definition = result.value();
      GURL url(task_definition.url());
      metadata_map_.emplace(url,
                            TaskDiscoveryService::AutomationMetadata{
                                .title = task_definition.title(),
                                .instructions = task_definition.description(),
                                .anchored_message = ""});
    }
  }

  for (auto& query : pending_queries_) {
    std::move(query.second).Run(GetMetadata(query.first));
  }
  pending_queries_.clear();
}

}  // namespace record_replay
