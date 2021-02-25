// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/file_system_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/common/file_system/file_system_types.h"

using content::BrowserThread;

namespace storage {
class FileSystemContext;
}

namespace browsing_data {

FileSystemHelper::FileSystemHelper(
    storage::FileSystemContext* filesystem_context,
    const std::vector<storage::FileSystemType>& additional_types)
    : filesystem_context_(filesystem_context) {
  for (storage::FileSystemType type : additional_types)
    types_.push_back(type);
  DCHECK(filesystem_context_.get());
}

FileSystemHelper::~FileSystemHelper() {}

base::SequencedTaskRunner* FileSystemHelper::file_task_runner() {
  return filesystem_context_->default_file_task_runner();
}

void FileSystemHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemHelper::FetchFileSystemInfoInFileThread, this,
                     std::move(callback)));
}

void FileSystemHelper::DeleteFileSystemOrigin(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemHelper::DeleteFileSystemOriginInFileThread,
                     this, origin));
}

void FileSystemHelper::FetchFileSystemInfoInFileThread(FetchCallback callback) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  std::list<FileSystemInfo> result;
  std::map<GURL, FileSystemInfo> file_system_info_map;
  for (storage::FileSystemType type : types_) {
    storage::FileSystemQuotaUtil* quota_util =
        filesystem_context_->GetQuotaUtil(type);
    DCHECK(quota_util);
    std::vector<url::Origin> origins =
        quota_util->GetOriginsForTypeOnFileTaskRunner(type);
    for (const auto& current : origins) {
      if (!HasWebScheme(current.GetURL()))
        continue;  // Non-websafe state is not considered browsing data.
      int64_t usage = quota_util->GetOriginUsageOnFileTaskRunner(
          filesystem_context_.get(), current, type);
      auto inserted =
          file_system_info_map
              .insert(std::make_pair(current.GetURL(), FileSystemInfo(current)))
              .first;
      inserted->second.usage_map[type] = usage;
    }
  }

  for (const auto& iter : file_system_info_map)
    result.push_back(iter.second);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void FileSystemHelper::DeleteFileSystemOriginInFileThread(
    const url::Origin& origin) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  filesystem_context_->DeleteDataForOriginOnFileTaskRunner(origin);
}

FileSystemHelper::FileSystemInfo::FileSystemInfo(const url::Origin& origin)
    : origin(origin) {}

FileSystemHelper::FileSystemInfo::FileSystemInfo(const FileSystemInfo& other) =
    default;

FileSystemHelper::FileSystemInfo::~FileSystemInfo() {}

// static
FileSystemHelper* FileSystemHelper::Create(
    storage::FileSystemContext* filesystem_context,
    const std::vector<storage::FileSystemType>& additional_types) {
  return new FileSystemHelper(filesystem_context, additional_types);
}

CannedFileSystemHelper::CannedFileSystemHelper(
    storage::FileSystemContext* filesystem_context,
    const std::vector<storage::FileSystemType>& additional_types)
    : FileSystemHelper(filesystem_context, additional_types) {}

CannedFileSystemHelper::~CannedFileSystemHelper() {}

void CannedFileSystemHelper::Add(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.
  pending_origins_.insert(origin);
}

void CannedFileSystemHelper::Reset() {
  pending_origins_.clear();
}

bool CannedFileSystemHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedFileSystemHelper::GetCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_origins_.size();
}

void CannedFileSystemHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<FileSystemInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void CannedFileSystemHelper::DeleteFileSystemOrigin(const url::Origin& origin) {
  pending_origins_.erase(origin);
  FileSystemHelper::DeleteFileSystemOrigin(origin);
}

}  // namespace browsing_data
