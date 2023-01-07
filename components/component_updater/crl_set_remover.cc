// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/crl_set_remover.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"

namespace component_updater {

void DeleteLegacyCRLSet(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeleteFileCallback(user_data_dir.Append(
          FILE_PATH_LITERAL("Certificate Revocation Lists"))));
}

}  // namespace component_updater
