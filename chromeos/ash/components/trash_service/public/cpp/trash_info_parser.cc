// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trash_service/public/cpp/trash_info_parser.h"

#include "base/files/file.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace ash::trash_service {

namespace {

base::File GetReadOnlyFileFromPath(const base::FilePath& path) {
  return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
}

}  // namespace

TrashInfoParser::TrashInfoParser() {
  auto trash_pending_remote = LaunchTrashService();
  service_ = mojo::Remote<mojom::TrashService>(std::move(trash_pending_remote));
}

TrashInfoParser::~TrashInfoParser() = default;

void TrashInfoParser::SetDisconnectHandler(
    base::OnceCallback<void()> disconnect_callback) {
  if (service_) {
    service_.set_disconnect_handler(std::move(disconnect_callback));
  }
}

void TrashInfoParser::ParseTrashInfoFile(const base::FilePath& path,
                                         ParseTrashInfoCallback callback) {
  if (!service_) {
    LOG(ERROR) << "Trash service is not connected";
    std::move(callback).Run(base::File::FILE_ERROR_FAILED, base::FilePath(),
                            base::Time());
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetReadOnlyFileFromPath, std::move(path)),
      base::BindOnce(&TrashInfoParser::OnGotFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TrashInfoParser::OnGotFile(ParseTrashInfoCallback callback,
                                base::File file) {
  if (!file.IsValid()) {
    LOG(ERROR) << "Trash info file is not valid " << file.error_details();
    std::move(callback).Run(file.error_details(), base::FilePath(),
                            base::Time());
    return;
  }
  service_->ParseTrashInfoFile(std::move(file), std::move(callback));
}

}  // namespace ash::trash_service
