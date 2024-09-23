// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/change_owners_task.h"

#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
namespace {

void UpdateOwner(const base::FilePath& path, UpdaterScope scope) {
  if (scope == UpdaterScope::kUser || path.empty()) {
    return;
  }

  base::stat_wrapper_t stat_info = {};
  if (base::File::Lstat(path, &stat_info) != 0) {
    VPLOG(1) << "Failed to lstat " << path.value();
    return;
  }

  if (stat_info.st_uid != 0 &&
      lchown(path.value().c_str(), 0, stat_info.st_gid) != 0) {
    VPLOG(1) << "Failed to lchown " << path.value();
  }
}

void ChangeOwners(scoped_refptr<PersistedData> persisted_data,
                  UpdaterScope scope,
                  base::OnceClosure callback) {
  std::vector<base::FilePath> files;
  std::vector<std::string> ids = persisted_data->GetAppIds();
  files.resize(ids.size());
  for (const std::string& id : ids) {
    files.push_back(persisted_data->GetExistenceCheckerPath(id));
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const std::vector<base::FilePath>& paths, UpdaterScope scope) {
            for (const base::FilePath& path : paths) {
              UpdateOwner(path, scope);
            }
          },
          files, scope),
      std::move(callback));
}

}  // namespace

base::OnceCallback<void(base::OnceClosure)> MakeChangeOwnersTask(
    scoped_refptr<PersistedData> persisted_data,
    UpdaterScope scope) {
  return base::BindOnce(&ChangeOwners, persisted_data, scope);
}

}  // namespace updater
