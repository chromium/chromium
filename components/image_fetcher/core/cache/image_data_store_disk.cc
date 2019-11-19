// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_data_store_disk.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

using base::File;
using base::FileEnumerator;
using base::FilePath;

namespace image_fetcher {

namespace {

// The prefix of the file names for images that need transcoding before use.
constexpr char kNeedsTranscodingPrefix[] = "ntr_";

const FilePath::CharType kPathPostfix[] =
    FILE_PATH_LITERAL("image_data_storage");

InitializationStatus InitializeImpl(FilePath storage_path) {
  // TODO(wylieb): Report errors if they occur.
  File::Error error;
  // This will check if the directory exists first.
  if (base::CreateDirectoryAndGetError(storage_path, &error)) {
    return InitializationStatus::INITIALIZED;
  }

  DVLOG(1) << "Initialization failed.";
  return InitializationStatus::INIT_FAILURE;
}

base::FilePath BuildFilePath(const FilePath& storage_path,
                             const std::string& key,
                             bool needs_transcoding) {
  if (needs_transcoding)
    return storage_path.AppendASCII(kNeedsTranscodingPrefix + key);
  return storage_path.AppendASCII(key);
}

void SaveImageImpl(FilePath storage_path,
                   const std::string& key,
                   std::string data,
                   bool needs_transcoding) {
  FilePath file_path = BuildFilePath(storage_path, key, needs_transcoding);

  int len = base::WriteFile(file_path, data.c_str(), data.length());
  if (len == -1 || (size_t)len != data.length()) {
    DVLOG(1) << "WriteFile failed.";
  }

  if (!needs_transcoding) {
    // Attempt to delete the image data there that needs transcoding.
    bool success = base::DeleteFile(
        BuildFilePath(storage_path, key, /* needs_transcoding */ true), false);
    if (!success) {
      DVLOG(1) << "Deleting the transcoded file failed.";
    }
  }
}

std::string LoadImageImpl(FilePath storage_path,
                          const std::string& key,
                          bool needs_transcoding) {
  FilePath file_path = BuildFilePath(storage_path, key, needs_transcoding);

  if (!base::PathExists(file_path)) {
    return "";
  }

  std::string data;
  base::ReadFileToString(file_path, &data);
  return data;
}

void DeleteImageImpl(FilePath storage_path, const std::string& key) {
  FilePath file_path =
      BuildFilePath(storage_path, key, /* needs_transcoding */ false);
  bool success = base::DeleteFile(file_path, false);
  if (success) {
    // We don't know if this image came from network or from an untranscoded
    // file. Instead of checking, we can simply blindly delete the untranscoded
    // file.
    if (!base::DeleteFile(
            BuildFilePath(storage_path, key, /* needs_transcoding */ true),
            false)) {
      DVLOG(1) << "Attempting to delete " << kNeedsTranscodingPrefix << ".";
    }
  } else {
    DVLOG(1) << "DeleteFile failed.";
  }
}

std::vector<std::string> GetAllKeysImpl(FilePath storage_path) {
  std::vector<std::string> keys;
  FileEnumerator file_enumerator(storage_path, false, FileEnumerator::FILES);
  for (FilePath name = file_enumerator.Next(); !name.empty();
       name = file_enumerator.Next()) {
    keys.push_back(name.BaseName().MaybeAsASCII());
  }

  return keys;
}

}  // namespace

// TODO(wylieb): Add histogram for failures.
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
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(InitializeImpl, storage_path_),
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

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
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

void ImageDataStoreDisk::GetAllKeys(KeysCallback callback) {
  if (!IsInitialized()) {
    std::move(callback).Run({});
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(GetAllKeysImpl, storage_path_), std::move(callback));
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
