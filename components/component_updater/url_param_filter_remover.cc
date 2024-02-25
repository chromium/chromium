// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/url_param_filter_remover.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace component_updater {

void DeleteUrlParamFilter(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          base::IgnoreResult(&base::DeletePathRecursively),
          user_data_dir.Append(FILE_PATH_LITERAL("UrlParamClassifications"))));
}

}  // namespace component_updater
