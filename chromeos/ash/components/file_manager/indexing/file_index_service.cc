// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/file_index_service.h"

#include <memory>

#include "base/task/thread_pool.h"
#include "chromeos/ash/components/file_manager/indexing/sql_storage.h"

namespace ash::file_manager {

// FileIndexService provides asynchronous version of operations defined in
// the FileIndex class. The current structure of the classes is as follows:
//
//  [ FileIndexService ]---<>[ SequenceBound<FileIndex> ]
//                                                |
//                                                |
//              [ IndexStorage ]<>----------------'
//              [ (interface)  ]
//                      ^
//                      |
//               -------+--------.
//               |               |
//        [ RamStorage ]   [ SqlStorage ]
namespace {

base::FilePath MakeDbPath(base::FilePath& profile_path) {
  return profile_path.AppendASCII("file_manager").AppendASCII("file_index.db");
}

constexpr char kSqlDatabaseUmaTag[] =
    "FileBrowser.FileIndex.SqlDatabase.Status";

}  // namespace

FileIndexService::FileIndexService(base::FilePath profile_path)
    : file_index_(base::ThreadPool::CreateSequencedTaskRunner(
                      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                       base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                  std::make_unique<SqlStorage>(MakeDbPath(profile_path),
                                               kSqlDatabaseUmaTag)) {}
FileIndexService::~FileIndexService() = default;

void FileIndexService::Init(IndexingOperationCallback callback) {
  if (inited_ != OpResults::kUndefined) {
    std::move(callback).Run(inited_);
    return;
  }
  file_index_.AsyncCall(&FileIndex::Init)
      .Then(base::BindOnce(
          [](IndexingOperationCallback callback, OpResults* inited,
             OpResults result) {
            *inited = result;
            std::move(callback).Run(result);
          },
          std::move(callback), &inited_));
}

void FileIndexService::PutFileInfo(const FileInfo& file_info,
                                   IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(OpResults::kUninitialized);
    return;
  }
  file_index_.AsyncCall(&FileIndex::PutFileInfo)
      .WithArgs(file_info)
      .Then(std::move(callback));
}

void FileIndexService::SetTerms(const std::vector<Term>& terms,
                                const GURL& url,
                                IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_.AsyncCall(&FileIndex::SetTerms)
      .WithArgs(terms, url)
      .Then(std::move(callback));
}

void FileIndexService::AddTerms(const std::vector<Term>& terms,
                                const GURL& url,
                                IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_.AsyncCall(&FileIndex::AddTerms)
      .WithArgs(terms, url)
      .Then(std::move(callback));
}

void FileIndexService::RemoveFile(const GURL& url,
                                  IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_.AsyncCall(&FileIndex::RemoveFile)
      .WithArgs(url)
      .Then(std::move(callback));
}

void FileIndexService::MoveFile(const GURL& old_url,
                                const GURL& new_url,
                                IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_.AsyncCall(&FileIndex::MoveFile)
      .WithArgs(old_url, new_url)
      .Then(std::move(callback));
}

void FileIndexService::RemoveTerms(const std::vector<Term>& terms,
                                   const GURL& url,
                                   IndexingOperationCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(kUninitialized);
    return;
  }
  file_index_.AsyncCall(&FileIndex::RemoveTerms)
      .WithArgs(terms, url)
      .Then(std::move(callback));
}

// Searches the index for file info matching the specified query.
void FileIndexService::Search(const Query& query,
                              SearchResultsCallback callback) {
  if (inited_ != OpResults::kSuccess) {
    std::move(callback).Run(SearchResults());
    return;
  }
  file_index_.AsyncCall(&FileIndex::Search)
      .WithArgs(query)
      .Then(std::move(callback));
}

}  // namespace ash::file_manager
