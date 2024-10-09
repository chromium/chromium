// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/filesystem/directory_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "components/services/filesystem/util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace filesystem {

DirectoryImpl::DirectoryImpl(base::FilePath directory_path,
                             scoped_refptr<SharedTempDir> temp_dir)
    : directory_path_(directory_path), temp_dir_(std::move(temp_dir)) {}

DirectoryImpl::~DirectoryImpl() = default;

void DirectoryImpl::Read(ReadCallback callback) {
  std::vector<mojom::DirectoryEntryPtr> entries;
  base::FileEnumerator directory_enumerator(
      directory_path_, false,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  for (base::FilePath path = directory_enumerator.Next(); !path.empty();
       path = directory_enumerator.Next()) {
    base::FileEnumerator::FileInfo info = directory_enumerator.GetInfo();
    mojom::DirectoryEntryPtr entry = mojom::DirectoryEntry::New();
    entry->type = info.IsDirectory() ? mojom::FsFileType::DIRECTORY
                                     : mojom::FsFileType::REGULAR_FILE;
    entry->name = path.BaseName();
    entry->display_name = info.GetName();

    entries.push_back(std::move(entry));
  }

  std::move(callback).Run(
      base::File::Error::FILE_OK,
      entries.empty() ? std::nullopt : std::make_optional(std::move(entries)));
}

// TODO(erg): Consider adding an implementation of Stat()/Touch() to the
// directory, too. Right now, the base::File abstractions do not really deal
// with directories properly, so these are broken for now.

void DirectoryImpl::OpenFileHandle(const std::string& raw_path,
                                   uint32_t open_flags,
                                   OpenFileHandleCallback callback) {
  base::File file = OpenFileHandleImpl(raw_path, open_flags);
  base::File::Error error = GetError(file);
  std::move(callback).Run(error, std::move(file));
}

void DirectoryImpl::OpenFileHandles(
    std::vector<mojom::FileOpenDetailsPtr> details,
    OpenFileHandlesCallback callback) {
  std::vector<mojom::FileOpenResultPtr> results(details.size());
  size_t i = 0;
  for (const auto& detail : details) {
    mojom::FileOpenResultPtr result(mojom::FileOpenResult::New());
    result->path = detail->path;
    result->file_handle = OpenFileHandleImpl(detail->path, detail->open_flags);
    result->error = GetError(result->file_handle);
    results[i++] = std::move(result);
  }
  std::move(callback).Run(std::move(results));
}

void DirectoryImpl::OpenDirectory(
    const std::string& raw_path,
    mojo::PendingReceiver<mojom::Directory> receiver,
    uint32_t open_flags,
    OpenDirectoryCallback callback) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  if (!base::DirectoryExists(path)) {
    if (base::PathExists(path)) {
      std::move(callback).Run(base::File::Error::FILE_ERROR_NOT_A_DIRECTORY);
      return;
    }

    if (!(open_flags & mojom::kFlagOpenAlways ||
          open_flags & mojom::kFlagCreate)) {
      // The directory doesn't exist, and we weren't passed parameters to
      // create it.
      std::move(callback).Run(base::File::Error::FILE_ERROR_NOT_FOUND);
      return;
    }

    if (!base::CreateDirectoryAndGetError(path, &error)) {
      std::move(callback).Run(error);
      return;
    }
  }

  if (receiver) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<DirectoryImpl>(path, temp_dir_), std::move(receiver));
  }

  std::move(callback).Run(base::File::Error::FILE_OK);
}

void DirectoryImpl::Rename(const std::string& raw_old_path,
                           const std::string& raw_new_path,
                           RenameCallback callback) {
  base::FilePath old_path;
  base::File::Error error =
      ValidatePath(raw_old_path, directory_path_, &old_path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  base::FilePath new_path;
  error = ValidatePath(raw_new_path, directory_path_, &new_path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  if (!base::Move(old_path, new_path)) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK);
}

void DirectoryImpl::Replace(const std::string& raw_old_path,
                            const std::string& raw_new_path,
                            ReplaceCallback callback) {
  base::FilePath old_path;
  base::File::Error error =
      ValidatePath(raw_old_path, directory_path_, &old_path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  base::FilePath new_path;
  error = ValidatePath(raw_new_path, directory_path_, &new_path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  base::File::Error file_error;
  if (!base::ReplaceFile(old_path, new_path, &file_error)) {
    std::move(callback).Run(file_error);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK);
}

void DirectoryImpl::Delete(const std::string& raw_path,
                           uint32_t delete_flags,
                           DeleteCallback callback) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  bool success;
  if (delete_flags & mojom::kDeleteFlagRecursive)
    success = base::DeletePathRecursively(path);
  else
    success = base::DeleteFile(path);
  if (!success) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK);
}

void DirectoryImpl::Exists(const std::string& raw_path,
                           ExistsCallback callback) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, false);
    return;
  }

  bool exists = base::PathExists(path);
  std::move(callback).Run(base::File::Error::FILE_OK, exists);
}

void DirectoryImpl::IsWritable(const std::string& raw_path,
                               IsWritableCallback callback) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, false);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK,
                          base::PathIsWritable(path));
}

void DirectoryImpl::Flush(FlushCallback callback) {
// On Windows no need to sync directories. Their metadata will be updated when
// files are created, without an explicit sync.
#if !BUILDFLAG(IS_WIN)
  base::File file(directory_path_,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    std::move(callback).Run(GetError(file));
    return;
  }

  if (!file.Flush()) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED);
    return;
  }
#endif
  std::move(callback).Run(base::File::Error::FILE_OK);
}

void DirectoryImpl::StatFile(const std::string& raw_path,
                             StatFileCallback callback) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, nullptr);
    return;
  }

  base::File base_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!base_file.IsValid()) {
    std::move(callback).Run(GetError(base_file), nullptr);
    return;
  }

  base::File::Info info;
  if (!base_file.GetInfo(&info)) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED, nullptr);
    return;
  }

  std::move(callback).Run(base::File::Error::FILE_OK,
                          MakeFileInformation(info));
}

void DirectoryImpl::Clone(mojo::PendingReceiver<mojom::Directory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DirectoryImpl>(directory_path_, temp_dir_),
      std::move(receiver));
}

void DirectoryImpl::ReadEntireFile(const std::string& raw_path,
                                   ReadEntireFileCallback callback) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error, std::vector<uint8_t>());
    return;
  }

  if (base::DirectoryExists(path)) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_NOT_A_FILE,
                            std::vector<uint8_t>());
    return;
  }

  base::File base_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!base_file.IsValid()) {
    std::move(callback).Run(GetError(base_file), std::vector<uint8_t>());
    return;
  }

  std::vector<uint8_t> contents;
  const int kBufferSize = 1 << 16;
  auto buf = base::HeapArray<char>::Uninit(kBufferSize);
  int len;
  while ((len = base_file.ReadAtCurrentPos(buf.data(), kBufferSize)) > 0) {
    contents.insert(contents.end(), buf.data(), buf.data() + len);
  }

  std::move(callback).Run(base::File::Error::FILE_OK, contents);
}

void DirectoryImpl::WriteFile(const std::string& raw_path,
                              const std::vector<uint8_t>& data,
                              WriteFileCallback callback) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  if (base::DirectoryExists(path)) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_NOT_A_FILE);
    return;
  }

  base::File base_file(path,
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!base_file.IsValid()) {
    std::move(callback).Run(GetError(base_file));
    return;
  }

  // If we're given empty data, we don't write and just truncate the file.
  if (data.size()) {
    const int data_size = static_cast<int>(data.size());
    if (base_file.Write(0, reinterpret_cast<const char*>(&data.front()),
                        data_size) == -1) {
      std::move(callback).Run(GetError(base_file));
      return;
    }
  }

  std::move(callback).Run(base::File::Error::FILE_OK);
}

base::File DirectoryImpl::OpenFileHandleImpl(const std::string& raw_path,
                                             uint32_t open_flags) {
  base::FilePath path;
  base::File::Error error = ValidatePath(raw_path, directory_path_, &path);
  if (error != base::File::Error::FILE_OK)
    return base::File(static_cast<base::File::Error>(error));

  if (base::DirectoryExists(path)) {
    // We must not return directories as files. In the file abstraction, we
    // can fetch raw file descriptors over mojo pipes, and passing a file
    // descriptor to a directory is a sandbox escape on Windows.
    return base::File(base::File::FILE_ERROR_NOT_A_FILE);
  }

  return base::File(path, open_flags);
}

}  // namespace filesystem
