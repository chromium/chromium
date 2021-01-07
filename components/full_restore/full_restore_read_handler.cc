// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_read_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/full_restore/full_restore_file_handler.h"
#include "components/full_restore/restore_data.h"

namespace full_restore {

FullRestoreReadHandler* FullRestoreReadHandler::GetInstance() {
  static base::NoDestructor<FullRestoreReadHandler> full_restore_read_handler;
  return full_restore_read_handler.get();
}

FullRestoreReadHandler::FullRestoreReadHandler() = default;

FullRestoreReadHandler::~FullRestoreReadHandler() = default;

void FullRestoreReadHandler::ReadFromFile(const base::FilePath& profile_path,
                                          Callback callback) {
  auto file_handler =
      base::MakeRefCounted<FullRestoreFileHandler>(profile_path);
  file_handler->owning_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FullRestoreFileHandler::ReadFromFile, file_handler.get()),
      base::BindOnce(&FullRestoreReadHandler::OnGetRestoreData,
                     weak_factory_.GetWeakPtr(), profile_path,
                     std::move(callback)));
}

void FullRestoreReadHandler::OnGetRestoreData(
    const base::FilePath& profile_path,
    Callback callback,
    std::unique_ptr<RestoreData> restore_data) {
  std::move(callback).Run(std::move(restore_data));
}

}  // namespace full_restore
