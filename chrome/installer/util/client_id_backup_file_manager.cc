// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/client_id_backup_file_manager.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"

namespace {

// File name used in the user data dir to store a backup of the client ID.
#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kClientIdBackupFileName[] =
    FILE_PATH_LITERAL("ClientIdBackup");
#else
// TODO(crbug.com/510249717): Migrate to the same file name that is used on the
// Windows platform.
constexpr base::FilePath::CharType kClientIdBackupFileName[] =
    FILE_PATH_LITERAL("Consent To Send Stats");
#endif

base::FilePath GetBackupFileDir() {
  return base::PathService::CheckedGet(chrome::DIR_USER_DATA);
}

base::FilePath GetBackupFilePath() {
  return GetBackupFileDir().Append(kClientIdBackupFileName);
}

void SetClientIdBackupFilePermissionIfNeeded(
    const base::FilePath& backup_file) {
#if BUILDFLAG(IS_CHROMEOS)
  // The backup file needs to be world-readable. See http://crbug.com/40079723
  int permissions;
  if (base::GetPosixFilePermissions(backup_file, &permissions) &&
      (permissions & base::FILE_PERMISSION_READ_BY_OTHERS) == 0) {
    permissions |= base::FILE_PERMISSION_READ_BY_OTHERS;
    base::SetPosixFilePermissions(backup_file, permissions);
  }
#endif
}

}  // namespace

// static
ClientIdBackupFileManager& ClientIdBackupFileManager::GetInstance() {
  static base::NoDestructor<ClientIdBackupFileManager> instance;
  return *instance;
}

// static
base::FilePath ClientIdBackupFileManager::GetBackupFilePathForTesting() {
  return GetBackupFilePath();
}

ClientIdBackupFileManager::ClientIdBackupFileManager() = default;

ClientIdBackupFileManager::~ClientIdBackupFileManager() = default;

std::optional<std::string>
ClientIdBackupFileManager::ClientIdFromCacheOrDisk() {
  auto cached_client_id = ClientIdFromCache();
  if (cached_client_id.has_value()) {
    return cached_client_id;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const base::FilePath backup_file_path = GetBackupFilePath();
  std::string client_id;

  // TODO(crbug.com/497679558): Use ReadFileToStringWithMaxSize() based on the
  // client ID format.
  if (!base::ReadFileToString(backup_file_path, &client_id)) {
    // The file doesn't exist or can't be read. Treat both cases as if UMA was
    // never activated.
    return std::nullopt;
  }

  // TODO(crbug.com/497679558): Validate the contents of the file before
  // assigning and returning it.
  base::AutoLock lock(client_id_lock_);
  client_id_ = std::move(client_id);

  return client_id_;
}

std::optional<std::string> ClientIdBackupFileManager::ClientIdFromCache()
    const {
  base::AutoLock lock(client_id_lock_);
  return client_id_;
}

bool ClientIdBackupFileManager::SetClientId(base::PassKey<GoogleUpdateSettings>,
                                            std::string client_id) {
  return SetClientIdInternal(std::move(client_id));
}

bool ClientIdBackupFileManager::SetClientIdForTesting(std::string client_id) {
  return SetClientIdInternal(std::move(client_id));
}

bool ClientIdBackupFileManager::SetClientIdInternal(
    std::optional<std::string> client_id) {
  {
    base::AutoLock lock(client_id_lock_);
    client_id_ = client_id;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const base::FilePath backup_file = GetBackupFilePath();

  bool persistence_update_success =
      client_id.has_value() ? base::WriteFile(backup_file, *client_id)
                            : base::DeleteFile(backup_file);

  if (!persistence_update_success) {
    return false;
  }

  if (client_id.has_value()) {
    SetClientIdBackupFilePermissionIfNeeded(backup_file);
  }
  return true;
}

bool ClientIdBackupFileManager::ClearClientId(
    base::PassKey<GoogleUpdateSettings> pass_key) {
  return SetClientIdInternal(std::nullopt);
}

bool ClientIdBackupFileManager::ClearClientIdForTesting() {
  return SetClientIdInternal(std::nullopt);
}

void ClientIdBackupFileManager::ResetForTesting() {
  base::AutoLock lock(client_id_lock_);
  client_id_ = std::nullopt;
}
