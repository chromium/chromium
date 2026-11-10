// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_data_store_disk.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"

using base::File;
using base::FileEnumerator;
using base::FilePath;

namespace image_fetcher {

namespace {

// The prefix of the file names for images that need transcoding before use.
constexpr char kNeedsTranscodingPrefix[] = "ntr_";

const FilePath::CharType kPathPostfix[] =
    FILE_PATH_LITERAL("image_data_storage");

bool IsStoragePathLink(const FilePath& storage_path) {
  return base::IsLink(storage_path) || base::IsLink(storage_path.DirName());
}

InitializationStatus InitializeImpl(FilePath storage_path) {
  File::Error error;
  // CreateDirectoryAndGetError() succeeds if `storage_path` is a symlink to an
  // existing directory, so check for links after it runs.
  if (base::CreateDirectoryAndGetError(storage_path, &error) &&
      !IsStoragePathLink(storage_path)) {
    ImageFetcherMetricsReporter::ReportCacheResult(
        ImageFetcherCacheResult::kDataStoreInitializationSucceeded);
    return InitializationStatus::INITIALIZED;
  }

  ImageFetcherMetricsReporter::ReportCacheResult(
      ImageFetcherCacheResult::kDataStoreInitializationFailed);
  DVLOG(1) << "Initialization failed.";
  return InitializationStatus::INIT_FAILURE;
}

base::FilePath BuildFilePath(const FilePath& storage_path,
                             const std::string& key,
                             bool needs_transcoding) {
  if (needs_transcoding) {
    return storage_path.AppendASCII(kNeedsTranscodingPrefix + key);
  }
  return storage_path.AppendASCII(key);
}

bool IsValidKey(const std::string& key) {
  // ImageCache supplies Base32 hashes; disk-store tests also use simple
  // filenames. Reject path separators and filesystem metacharacters.
  if (key.empty() || !std::ranges::all_of(key, [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-';
      })) {
    return false;
  }
  return true;
}

void SaveImageImpl(FilePath storage_path,
                   const std::string& key,
                   std::string data,
                   bool needs_transcoding) {
  if (IsStoragePathLink(storage_path) || !IsValidKey(key)) {
    ImageFetcherMetricsReporter::ReportCacheResult(
        ImageFetcherCacheResult::kDataStoreWriteFailed);
    DVLOG(1) << "Storage path or image key is invalid.";
    return;
  }

  FilePath file_path = BuildFilePath(storage_path, key, needs_transcoding);

  // FLAG_NO_FOLLOW maps to O_NOFOLLOW on POSIX (Android, ChromeOS, Linux,
  // Mac, iOS) and is ignored on Windows.
  File file(file_path,
            File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE | File::FLAG_NO_FOLLOW);
  const bool write_succeeded =
      file.IsValid() && file.WriteAndCheck(0, base::as_byte_span(data));
  ImageFetcherMetricsReporter::ReportCacheResult(
      write_succeeded ? ImageFetcherCacheResult::kDataStoreWriteSucceeded
                      : ImageFetcherCacheResult::kDataStoreWriteFailed);
  if (!write_succeeded) {
    DVLOG(1) << "WriteFile failed.";
    file.Close();
    base::DeleteFile(file_path);
    return;
  }

  if (!needs_transcoding) {
    // Attempt to delete the image data there that needs transcoding.
    bool success = base::DeleteFile(
        BuildFilePath(storage_path, key, /* needs_transcoding */ true));
    ImageFetcherMetricsReporter::ReportCacheResult(
        success ? ImageFetcherCacheResult::kDataStoreDeleteSucceeded
                : ImageFetcherCacheResult::kDataStoreDeleteFailed);
    if (!success) {
      DVLOG(1) << "Deleting the transcoded file failed.";
    }
  }
}

std::string LoadImageImpl(FilePath storage_path,
                          const std::string& key,
                          bool needs_transcoding) {
  if (IsStoragePathLink(storage_path) || !IsValidKey(key)) {
    ImageFetcherMetricsReporter::ReportCacheResult(
        ImageFetcherCacheResult::kDataStoreReadFailed);
    return "";
  }

  FilePath file_path = BuildFilePath(storage_path, key, needs_transcoding);

  // Open explicitly to distinguish a cache miss from other file errors.
  // FLAG_NO_FOLLOW maps to O_NOFOLLOW on POSIX and is ignored on Windows.
  File file(file_path,
            File::FLAG_OPEN | File::FLAG_READ | File::FLAG_NO_FOLLOW);
  if (!file.IsValid()) {
    // A missing path can be an absent image or its storage directory. Checking
    // the directory here would add a filesystem query to every cache miss.
    ImageFetcherMetricsReporter::ReportCacheResult(
        file.error_details() == File::FILE_ERROR_NOT_FOUND
            ? ImageFetcherCacheResult::kDataStoreReadNotFound
            : ImageFetcherCacheResult::kDataStoreReadFailed);
    return "";
  }

  base::ScopedFILE stream(base::FileToFILE(std::move(file), "rb"));
  std::string data;
  if (!stream || !base::ReadStreamToString(stream.get(), &data)) {
    ImageFetcherMetricsReporter::ReportCacheResult(
        ImageFetcherCacheResult::kDataStoreReadFailed);
    return "";
  }
  ImageFetcherMetricsReporter::ReportCacheResult(
      ImageFetcherCacheResult::kDataStoreReadSucceeded);
  return data;
}

void DeleteImageImpl(FilePath storage_path, const std::string& key) {
  if (IsStoragePathLink(storage_path) || !IsValidKey(key)) {
    ImageFetcherMetricsReporter::ReportCacheResult(
        ImageFetcherCacheResult::kDataStoreDeleteFailed);
    return;
  }

  FilePath file_path =
      BuildFilePath(storage_path, key, /* needs_transcoding */ false);
  bool success = base::DeleteFile(file_path);
  ImageFetcherMetricsReporter::ReportCacheResult(
      success ? ImageFetcherCacheResult::kDataStoreDeleteSucceeded
              : ImageFetcherCacheResult::kDataStoreDeleteFailed);
  if (success) {
    // We don't know if this image came from network or from an untranscoded
    // file. Instead of checking, we can simply blindly delete the untranscoded
    // file.
    const bool transcoding_delete_succeeded = base::DeleteFile(
        BuildFilePath(storage_path, key, /* needs_transcoding */ true));
    ImageFetcherMetricsReporter::ReportCacheResult(
        transcoding_delete_succeeded
            ? ImageFetcherCacheResult::kDataStoreDeleteSucceeded
            : ImageFetcherCacheResult::kDataStoreDeleteFailed);
    if (!transcoding_delete_succeeded) {
      DVLOG(1) << "Attempting to delete " << kNeedsTranscodingPrefix << ".";
    }
  } else {
    DVLOG(1) << "DeleteFile failed.";
  }
}

std::optional<std::vector<std::string>> GetAllKeysImpl(FilePath storage_path) {
  if (IsStoragePathLink(storage_path)) {
    ImageFetcherMetricsReporter::ReportCacheResult(
        ImageFetcherCacheResult::kDataStoreReadFailed);
    return std::nullopt;
  }

  std::vector<std::string> keys;
  // Preserve enumeration errors so they can be reported after the loop.
  FileEnumerator file_enumerator(storage_path, false, FileEnumerator::FILES,
                                 FilePath::StringType(),
                                 FileEnumerator::FolderSearchPolicy::ALL,
                                 FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    keys.push_back(name.BaseName().MaybeAsASCII());
  }
  // On Windows, an empty directory can report FILE_ERROR_NOT_FOUND. Treat it
  // as an error only if the storage directory itself is missing.
  const bool enumeration_failed =
      file_enumerator.GetError() != File::FILE_OK &&
      (file_enumerator.GetError() != File::FILE_ERROR_NOT_FOUND ||
       !base::DirectoryExists(storage_path));
  if (enumeration_failed) {
    ImageFetcherMetricsReporter::ReportCacheResult(
        ImageFetcherCacheResult::kDataStoreReadFailed);
    return std::nullopt;
  }
  ImageFetcherMetricsReporter::ReportCacheResult(
      ImageFetcherCacheResult::kDataStoreReadSucceeded);
  return keys;
}

}  // namespace

ImageDataStoreDisk::ImageDataStoreDisk(
    FilePath generic_storage_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : initialization_status_(InitializationStatus::UNINITIALIZED),
      task_runner_(task_runner) {
  storage_path_ = generic_storage_path.Append(kPathPostfix);
}

ImageDataStoreDisk::~ImageDataStoreDisk() = default;

void ImageDataStoreDisk::Initialize(base::OnceClosure callback) {
  DCHECK(initialization_status_ == InitializationStatus::UNINITIALIZED);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(InitializeImpl, storage_path_),
      base::BindOnce(&ImageDataStoreDisk::OnInitializationComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ImageDataStoreDisk::IsInitialized() {
  return initialization_status_ == InitializationStatus::INITIALIZED;
}

void ImageDataStoreDisk::SaveImage(const std::string& key,
                                   std::string image_data,
                                   bool needs_transcoding) {
  if (!IsInitialized()) {
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SaveImageImpl, storage_path_, key,
                                std::move(image_data), needs_transcoding));
}

void ImageDataStoreDisk::LoadImage(const std::string& key,
                                   bool needs_transcoding,
                                   ImageDataCallback callback) {
  if (!IsInitialized()) {
    std::move(callback).Run(/* needs_transcoding */ false, std::string());
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(LoadImageImpl, storage_path_, key, needs_transcoding),
      base::BindOnce(&ImageDataStoreDisk::OnImageLoaded,
                     weak_ptr_factory_.GetWeakPtr(), needs_transcoding,
                     std::move(callback)));
}

void ImageDataStoreDisk::DeleteImage(const std::string& key) {
  if (!IsInitialized()) {
    return;
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(DeleteImageImpl, storage_path_, key));
}

void ImageDataStoreDisk::GetAllKeys(StoreKeysCallback callback) {
  if (!IsInitialized()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(GetAllKeysImpl, storage_path_),
      std::move(callback));
}

void ImageDataStoreDisk::OnInitializationComplete(
    base::OnceClosure callback,
    InitializationStatus initialization_status) {
  initialization_status_ = initialization_status;
  std::move(callback).Run();
}

void ImageDataStoreDisk::OnImageLoaded(bool needs_transcoding,
                                       ImageDataCallback callback,
                                       std::string data) {
  std::move(callback).Run(needs_transcoding, std::move(data));
}

}  // namespace image_fetcher
