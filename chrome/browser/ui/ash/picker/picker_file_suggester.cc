// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_file_suggester.h"

#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/files/file_title.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"

namespace {

constexpr base::TimeDelta kMaxFileRecencyDelta = base::Days(30);

storage::FileSystemContext* GetFileSystemContextForProfile(Profile* profile) {
  content::StoragePartition* storage = profile->GetDefaultStoragePartition();
  return storage->GetFileSystemContext();
}

}  // namespace

PickerFileSuggester::PickerFileSuggester(Profile* profile)
    : profile_(profile) {}

PickerFileSuggester::~PickerFileSuggester() = default;

void PickerFileSuggester::GetRecentLocalFiles(
    RecentLocalFilesCallback callback) {
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      GetFileSystemContextForProfile(profile_);
  if (!file_system_context) {
    return;
  }

  ash::RecentModel* model = ash::RecentModelFactory::GetForProfile(profile_);
  if (!model) {
    return;
  }

  model->GetRecentFiles(
      file_system_context.get(), GURL(), /*query=*/"", kMaxFileRecencyDelta,
      ash::RecentModel::FileType::kAll,
      /*invalidate_cache=*/false,
      base::BindOnce(&PickerFileSuggester::OnGetRecentLocalFiles,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PickerFileSuggester::GetRecentDriveFiles(
    RecentDriveFilesCallback callback) {
  // TODO: b/330634632 - Implement recent Drive file results.
  NOTIMPLEMENTED_LOG_ONCE();
  std::move(callback).Run({});
}

void PickerFileSuggester::OnGetRecentLocalFiles(
    RecentLocalFilesCallback callback,
    const std::vector<ash::RecentFile>& recent_files) {
  std::vector<LocalFile> files;
  files.reserve(recent_files.size());
  for (const ash::RecentFile& recent_file : recent_files) {
    const storage::FileSystemURL& url = recent_file.url();
    if (url.type() == storage::kFileSystemTypeLocal) {
      const base::FilePath& path = url.path();
      files.push_back({.title = app_list::GetFileTitle(path), .path = path});
    }
  }
  std::move(callback).Run(std::move(files));
}
