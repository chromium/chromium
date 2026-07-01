// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_space_check.h"

#include <cstdint>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/pipeline_util.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace update_client {

namespace {

int64_t CalculateRequiredDiskSpace(const ProtocolParser::Pipeline& pipeline) {
  int64_t download_size = 0;
  int64_t expected_crx_size = 0;
  for (const ProtocolParser::Operation& op : pipeline.operations) {
    if (op.type == "download") {
      download_size += op.size;
    } else if (op.type == "crx3") {
      expected_crx_size += op.size;
    }
  }

  // Always require space for the downloaded content and the extracted CRX
  // contents (estimated as the size of the CRX itself). For differential
  // updates, also require space for the reconstructed CRX.
  return download_size + expected_crx_size +
         (download_size != expected_crx_size ? expected_crx_size : 0);
}

}  // namespace

base::OnceClosure SpaceCheckOperation(
    const ProtocolParser::Pipeline& pipeline,
    base::RepeatingCallback<int64_t(const base::FilePath&)> get_available_space,
    base::RepeatingCallback<void(base::DictValue)> event_adder,
    const base::FilePath& path,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](base::RepeatingCallback<int64_t(const base::FilePath&)>
                 get_available_space) {
            base::ScopedTempDir temp_dir;
            return CreateScopedTempDirectory(temp_dir)
                       ? get_available_space.Run(temp_dir.GetPath())
                       : int64_t{0};
          },
          get_available_space),
      base::BindOnce(
          [](int64_t required_bytes,
             base::RepeatingCallback<void(base::DictValue)> event_adder,
             base::OnceCallback<void(
                 base::expected<base::FilePath, CategorizedError>)> callback,
             const base::FilePath& path, int64_t available_bytes) {
            if (available_bytes < required_bytes) {
              VLOG(1) << "available_bytes: " << available_bytes
                      << ", required_bytes: " << required_bytes;
              event_adder.Run(MakeSimpleOperationEvent(
                  CategorizedError({.category = ErrorCategory::kDownload,
                                    .code = static_cast<int>(
                                        CrxDownloaderError::DISK_FULL)}),
                  protocol_request::kEventDownload));
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback),
                                 base::unexpected<CategorizedError>(
                                     {.category = ErrorCategory::kDownload,
                                      .code = static_cast<int>(
                                          CrxDownloaderError::DISK_FULL)})));
              return;
            }
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), path));
          },
          CalculateRequiredDiskSpace(pipeline), event_adder,
          std::move(callback), path));
  // This operation does not support cancellation.
  return base::DoNothing();
}

}  // namespace update_client
