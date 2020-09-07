// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/task/post_task.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace storage {

namespace {

size_t GetNumPathComponents(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  return components.size();
}

class LocalFileLockImpl : public FilesystemProxy::FileLock {
 public:
  LocalFileLockImpl(base::FilePath path, base::File lock)
      : path_(std::move(path)), lock_(std::move(lock)) {}
  ~LocalFileLockImpl() override {
    if (lock_.IsValid())
      Release();
  }

  // FilesystemProxy::FileLock implementation:
  base::File::Error Release() override {
    base::File::Error error = base::File::FILE_OK;
#if !defined(OS_FUCHSIA)
    error = lock_.Unlock();
#endif
    lock_.Close();
    FilesystemImpl::UnlockFileLocal(path_);
    return error;
  }

 private:
  const base::FilePath path_;
  base::File lock_;
};

class RemoteFileLockImpl : public FilesystemProxy::FileLock {
 public:
  explicit RemoteFileLockImpl(mojo::PendingRemote<mojom::FileLock> remote_lock)
      : remote_lock_(std::move(remote_lock)) {}
  ~RemoteFileLockImpl() override {
    if (remote_lock_)
      Release();
  }

  // FilesystemProxy::FileLock implementation:
  base::File::Error Release() override {
    DCHECK(remote_lock_);
    base::File::Error error = base::File::FILE_ERROR_IO;
    mojo::Remote<mojom::FileLock>(std::move(remote_lock_))->Release(&error);
    return error;
  }

 private:
  mojo::PendingRemote<mojom::FileLock> remote_lock_;
};

}  // namespace

FilesystemProxy::FilesystemProxy(decltype(UNRESTRICTED),
                                 const base::FilePath& root)
    : root_(root) {
  DCHECK(root_.IsAbsolute() || root_.empty());
}

FilesystemProxy::FilesystemProxy(
    decltype(RESTRICTED),
    const base::FilePath& root,
    mojo::PendingRemote<mojom::Directory> directory,
    scoped_refptr<base::SequencedTaskRunner> ipc_task_runner)
    : root_(root),
      num_root_components_(GetNumPathComponents(root_)),
      remote_directory_(std::move(directory), ipc_task_runner) {
  DCHECK(root_.IsAbsolute());
}

FilesystemProxy::~FilesystemProxy() = default;

bool FilesystemProxy::PathExists(const base::FilePath& path) {
  if (!remote_directory_)
    return base::PathExists(MaybeMakeAbsolute(path));

  bool exists = false;
  remote_directory_->PathExists(MakeRelative(path), &exists);
  return exists;
}

FileErrorOr<std::vector<base::FilePath>> FilesystemProxy::GetDirectoryEntries(
    const base::FilePath& path,
    DirectoryEntryType type) {
  const mojom::GetEntriesMode mode =
      type == DirectoryEntryType::kFilesOnly
          ? mojom::GetEntriesMode::kFilesOnly
          : mojom::GetEntriesMode::kFilesAndDirectories;
  if (!remote_directory_)
    return FilesystemImpl::GetDirectoryEntries(MaybeMakeAbsolute(path), mode);

  base::File::Error error = base::File::FILE_ERROR_IO;
  std::vector<base::FilePath> entries;
  remote_directory_->GetEntries(MakeRelative(path), mode, &error, &entries);
  if (error != base::File::FILE_OK)
    return error;

  // Fix up all the relative paths to be absolute.
  const base::FilePath root = path.IsAbsolute() ? path : root_.Append(path);
  for (auto& entry : entries)
    entry = root.Append(entry);
  return entries;
}

FileErrorOr<base::File> FilesystemProxy::OpenFile(const base::FilePath& path,
                                                  int flags) {
  if (!remote_directory_) {
    base::File file(MaybeMakeAbsolute(path), flags);
    if (!file.IsValid())
      return file.error_details();
    return file;
  }

  // NOTE: Remote directories only support a subset of |flags| values.
  const int kModeMask = base::File::FLAG_OPEN | base::File::FLAG_CREATE |
                        base::File::FLAG_OPEN_ALWAYS |
                        base::File::FLAG_CREATE_ALWAYS |
                        base::File::FLAG_OPEN_TRUNCATED;
  const int kWriteMask = base::File::FLAG_WRITE | base::File::FLAG_APPEND;
  const int kSupportedFlagsMask =
      kModeMask | kWriteMask | base::File::FLAG_READ;
  DCHECK((flags & ~kSupportedFlagsMask) == 0) << "Unsupported flags: " << flags;

  const int mode_flags = flags & kModeMask;
  mojom::FileOpenMode mode;
  switch (mode_flags) {
    case base::File::FLAG_OPEN:
      mode = mojom::FileOpenMode::kOpenIfExists;
      break;
    case base::File::FLAG_CREATE:
      mode = mojom::FileOpenMode::kCreateAndOpenOnlyIfNotExists;
      break;
    case base::File::FLAG_OPEN_ALWAYS:
      mode = mojom::FileOpenMode::kAlwaysOpen;
      break;
    case base::File::FLAG_CREATE_ALWAYS:
      mode = mojom::FileOpenMode::kAlwaysCreate;
      break;
    case base::File::FLAG_OPEN_TRUNCATED:
      mode = mojom::FileOpenMode::kOpenIfExistsAndTruncate;
      break;
    default:
      NOTREACHED() << "Invalid open mode flags: " << mode_flags;
      return base::File::FILE_ERROR_FAILED;
  }

  mojom::FileReadAccess read_access =
      (flags & base::File::FLAG_READ) != 0
          ? mojom::FileReadAccess::kReadAllowed
          : mojom::FileReadAccess::kReadNotAllowed;

  const int write_flags = flags & kWriteMask;
  mojom::FileWriteAccess write_access;
  switch (write_flags) {
    case 0:
      write_access = mojom::FileWriteAccess::kWriteNotAllowed;
      break;
    case base::File::FLAG_WRITE:
      write_access = mojom::FileWriteAccess::kWriteAllowed;
      break;
    case base::File::FLAG_APPEND:
      write_access = mojom::FileWriteAccess::kAppendOnly;
      break;
    default:
      NOTREACHED() << "Invalid write access flags: " << write_flags;
      return base::File::FILE_ERROR_FAILED;
  }

  base::File::Error error = base::File::FILE_ERROR_IO;
  base::File file;
  remote_directory_->OpenFile(MakeRelative(path), mode, read_access,
                              write_access, &error, &file);
  if (error != base::File::FILE_OK)
    return error;
  return file;
}

bool FilesystemProxy::WriteFileAtomically(const base::FilePath& path,
                                          const std::string& contents) {
  if (!remote_directory_) {
    return base::ImportantFileWriter::WriteFileAtomically(
        MaybeMakeAbsolute(path), contents);
  }

  bool success = false;
  remote_directory_->WriteFileAtomically(MakeRelative(path), contents,
                                         &success);
  return success;
}

base::File::Error FilesystemProxy::CreateDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_ERROR_IO;
  if (!remote_directory_) {
    if (!base::CreateDirectoryAndGetError(MaybeMakeAbsolute(path), &error))
      return error;
    return base::File::FILE_OK;
  }

  remote_directory_->CreateDirectory(MakeRelative(path), &error);
  return error;
}

bool FilesystemProxy::DeleteFile(const base::FilePath& path) {
  if (!remote_directory_) {
    const base::FilePath full_path = MaybeMakeAbsolute(path);
    return base::DeleteFile(full_path);
  }

  bool success = false;
  remote_directory_->DeleteFile(MakeRelative(path), &success);
  return success;
}

bool FilesystemProxy::DeletePathRecursively(const base::FilePath& path) {
  if (!remote_directory_) {
    const base::FilePath full_path = MaybeMakeAbsolute(path);
    return base::DeletePathRecursively(full_path);
  }

  bool success = false;
  remote_directory_->DeletePathRecursively(MakeRelative(path), &success);
  return success;
}

base::Optional<base::File::Info> FilesystemProxy::GetFileInfo(
    const base::FilePath& path) {
  if (!remote_directory_) {
    base::File::Info info;
    if (base::GetFileInfo(MaybeMakeAbsolute(path), &info))
      return info;
    return base::nullopt;
  }

  base::Optional<base::File::Info> info;
  remote_directory_->GetFileInfo(MakeRelative(path), &info);
  return info;
}

base::Optional<FilesystemProxy::PathAccessInfo> FilesystemProxy::GetPathAccess(
    const base::FilePath& path) {
  mojom::PathAccessInfoPtr info;
  if (!remote_directory_)
    info = FilesystemImpl::GetPathAccessLocal(MaybeMakeAbsolute(path));
  else
    remote_directory_->GetPathAccess(MakeRelative(path), &info);

  if (!info)
    return base::nullopt;

  return PathAccessInfo{info->can_read, info->can_write};
}

base::Optional<int> FilesystemProxy::GetMaximumPathComponentLength(
    const base::FilePath& path) {
  if (!remote_directory_)
    return base::GetMaximumPathComponentLength(MaybeMakeAbsolute(path));

  int len = -1;
  bool success = false;
  remote_directory_->GetMaximumPathComponentLength(MakeRelative(path), &success,
                                                   &len);
  if (!success)
    return base::nullopt;
  return len;
}

base::File::Error FilesystemProxy::RenameFile(const base::FilePath& old_path,
                                              const base::FilePath& new_path) {
  base::File::Error error = base::File::FILE_ERROR_IO;
  if (!remote_directory_) {
    if (!base::ReplaceFile(MaybeMakeAbsolute(old_path),
                           MaybeMakeAbsolute(new_path), &error)) {
      return error;
    }
    return base::File::FILE_OK;
  }

  remote_directory_->RenameFile(MakeRelative(old_path), MakeRelative(new_path),
                                &error);
  return error;
}

FileErrorOr<std::unique_ptr<FilesystemProxy::FileLock>>
FilesystemProxy::LockFile(const base::FilePath& path) {
  if (!remote_directory_) {
    base::FilePath full_path = MaybeMakeAbsolute(path);
    FileErrorOr<base::File> result = FilesystemImpl::LockFileLocal(full_path);
    if (result.is_error())
      return result.error();
    std::unique_ptr<FileLock> lock = std::make_unique<LocalFileLockImpl>(
        std::move(full_path), std::move(result.value()));
    return lock;
  }

  mojo::PendingRemote<mojom::FileLock> remote_lock;
  base::File::Error error = base::File::FILE_ERROR_IO;
  if (!remote_directory_->LockFile(MakeRelative(path), &error, &remote_lock))
    return error;
  if (error != base::File::FILE_OK)
    return error;

  std::unique_ptr<FileLock> lock =
      std::make_unique<RemoteFileLockImpl>(std::move(remote_lock));
  return lock;
}

bool FilesystemProxy::SetOpenedFileLength(base::File* file, uint64_t length) {
  if (!remote_directory_)
    return file->SetLength(length);

  bool success = false;
  remote_directory_->SetOpenedFileLength(std::move(*file), length, &success,
                                         file);
  return success;
}

// TODO(enne): this could be a lot of sync ipcs.  Should this be implemented
// as a Directory API instead?
int64_t FilesystemProxy::ComputeDirectorySize(const base::FilePath& path) {
  if (!remote_directory_)
    return base::ComputeDirectorySize(MaybeMakeAbsolute(path));

  int64_t running_size = 0;

  const mojom::GetEntriesMode mode = mojom::GetEntriesMode::kFilesOnly;
  base::File::Error error = base::File::FILE_ERROR_IO;
  std::vector<base::FilePath> entries;
  base::FilePath relative_path = MakeRelative(path);
  remote_directory_->GetEntries(relative_path, mode, &error, &entries);
  if (error != base::File::FILE_OK)
    return running_size;

  for (auto& entry : entries) {
    base::Optional<base::File::Info> info;
    base::FilePath path = entry;
    remote_directory_->GetFileInfo(relative_path.Append(entry), &info);
    if (info.has_value())
      running_size += info->size;
  }

  return running_size;
}

base::FilePath FilesystemProxy::MakeRelative(const base::FilePath& path) const {
  DCHECK(remote_directory_);
  DCHECK(!path.ReferencesParent());

  // For a remote Directory, returned paths must always be relative to |root|.
  if (!path.IsAbsolute() || path.empty())
    return path;

  if (path == root_)
    return base::FilePath();

  // Absolute paths need to be rebased onto |root_|.
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  base::FilePath relative_path;
  for (size_t i = num_root_components_; i < components.size(); ++i)
    relative_path = relative_path.Append(components[i]);
  return relative_path;
}

base::FilePath FilesystemProxy::MaybeMakeAbsolute(
    const base::FilePath& path) const {
  DCHECK(!remote_directory_);
  if (path.IsAbsolute() || root_.empty())
    return path;

  return root_.Append(path);
}

}  // namespace storage
