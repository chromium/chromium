// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/autofill_states_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace component_updater {

void DeleteAutofillStatesComponent(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          base::IgnoreResult(&base::DeletePathRecursively),
          user_data_dir.Append(FILE_PATH_LITERAL("AutofillStates"))));
}

}  // namespace component_updater
