// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace storage {

namespace {

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
#if !BUILDFLAG(IS_FUCHSIA)
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

base::FileErrorOr<std::vector<base::FilePath>>
FilesystemProxy::GetDirectoryEntries(const base::FilePath& path,
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
    return base::unexpected(error);

  // Fix up all the relative paths to be absolute.
  const base::FilePath root = path.IsAbsolute() ? path : root_.Append(path);
  for (auto& entry : entries)
    entry = root.Append(entry);
  return entries;
}

base::FileErrorOr<base::File> FilesystemProxy::OpenFile(
    const base::FilePath& path,
    int flags) {
  if (!remote_directory_) {
    base::File file(MaybeMakeAbsolute(path), flags);
    if (!file.IsValid())
      return base::unexpected(file.error_details());
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
      NOTREACHED_IN_MIGRATION() << "Invalid open mode flags: " << mode_flags;
      return base::unexpected(base::File::FILE_ERROR_FAILED);
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
      NOTREACHED_IN_MIGRATION()
          << "Invalid write access flags: " << write_flags;
      return base::unexpected(base::File::FILE_ERROR_FAILED);
  }

  base::File::Error error = base::File::FILE_ERROR_IO;
  base::File file;
  remote_directory_->OpenFile(MakeRelative(path), mode, read_access,
                              write_access, &error, &file);
  if (error != base::File::FILE_OK)
    return base::unexpected(error);
  return file;
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

std::optional<base::File::Info> FilesystemProxy::GetFileInfo(
    const base::FilePath& path) {
  if (!remote_directory_) {
    base::File::Info info;
    if (base::GetFileInfo(MaybeMakeAbsolute(path), &info))
      return info;
    return std::nullopt;
  }

  std::optional<base::File::Info> info;
  remote_directory_->GetFileInfo(MakeRelative(path), &info);
  return info;
}

std::optional<FilesystemProxy::PathAccessInfo> FilesystemProxy::GetPathAccess(
    const base::FilePath& path) {
  mojom::PathAccessInfoPtr info;
  if (!remote_directory_)
    info = FilesystemImpl::GetPathAccessLocal(MaybeMakeAbsolute(path));
  else
    remote_directory_->GetPathAccess(MakeRelative(path), &info);

  if (!info)
    return std::nullopt;

  return PathAccessInfo{info->can_read, info->can_write};
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

base::FileErrorOr<std::unique_ptr<FilesystemProxy::FileLock>>
FilesystemProxy::LockFile(const base::FilePath& path,
                          bool* same_process_failure) {
  if (!remote_directory_) {
    base::FilePath full_path = MaybeMakeAbsolute(path);
    ASSIGN_OR_RETURN(base::File result, FilesystemImpl::LockFileLocal(
                                            full_path, same_process_failure));
    return std::make_unique<LocalFileLockImpl>(std::move(full_path),
                                               std::move(result));
  }

  mojo::PendingRemote<mojom::FileLock> remote_lock;
  base::File::Error error = base::File::FILE_ERROR_IO;
  if (!remote_directory_->LockFile(MakeRelative(path), &error, &remote_lock))
    return base::unexpected(error);
  if (error != base::File::FILE_OK)
    return base::unexpected(error);

  return std::make_unique<RemoteFileLockImpl>(std::move(remote_lock));
}

base::FilePath FilesystemProxy::MakeRelative(const base::FilePath& path) const {
  DCHECK(remote_directory_);
  DCHECK(!path.ReferencesParent());

  // For a remote Directory, returned paths must always be relative to |root|.
  if (!path.IsAbsolute() || path.empty())
    return path;

  if (path == root_)
    return base::FilePath();

  base::FilePath relative_path;
  CHECK(root_.AppendRelativePath(path, &relative_path))
      << " Failed making " << path << " relative to " << root_;
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
