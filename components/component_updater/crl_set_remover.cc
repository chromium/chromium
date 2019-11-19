// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/crl_set_remover.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"

namespace component_updater {

void DeleteLegacyCRLSet(const base::FilePath& user_data_dir) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                     user_data_dir.Append(
                         FILE_PATH_LITERAL("Certificate Revocation Lists")),
                     false));
}

}  // namespace component_updater
