// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/op_zucchini.h"
#include "components/update_client/pipeline_util.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "components/zucchini/zucchini.h"

namespace update_client {

namespace {

void Done(base::OnceCallback<
              void(base::expected<base::FilePath, CategorizedError>)> callback,
          base::RepeatingCallback<void(base::DictValue)> event_adder,
          const base::FilePath& out_file,
          bool success) {
  const auto result =
      success ? base::expected<base::FilePath, CategorizedError>(out_file)
              : base::unexpected<CategorizedError>(
                    {.category = ErrorCategory::kUnpack,
                     .code = static_cast<int>(UnpackerError::kXzFailed)});

  base::OnceClosure done = base::BindOnce(
      [](const base::expected<base::FilePath, CategorizedError>& result,
         base::OnceCallback<void(
             base::expected<base::FilePath, CategorizedError>)> callback,
         base::RepeatingCallback<void(base::DictValue)> event_adder) {
        event_adder.Run(
            MakeSimpleOperationEvent(result, protocol_request::kEventXz));
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), result));
      },
      result, std::move(callback), event_adder);

  if (result.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             std::move(done));
    return;
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
          [](const base::FilePath& out_file) {
            DeleteFileAndEmptyParentDirectory(out_file);
          },
          out_file),
      std::move(done));
}

}  // namespace

base::OnceClosure XzOperation(
    std::unique_ptr<Unzipper> unzipper,
    base::RepeatingCallback<void(base::DictValue)> event_adder,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    const base::FilePath& in_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  state_tracker.Run(ComponentState::kDecompressing);
  base::FilePath dest_file = in_file.DirName().AppendUTF8("decoded_xz");
  Unzipper* unzipper_raw = unzipper.get();
  return unzipper_raw->DecodeXz(
      in_file, dest_file,
      base::BindOnce(
          [](const base::FilePath& in_file, std::unique_ptr<Unzipper> unzipper,
             bool result) {
            RetryFileOperation(&base::DeleteFile, in_file);
            return result;
          },
          in_file, std::move(unzipper))
          .Then(base::BindPostTaskToCurrentDefault(base::BindOnce(
              &Done, std::move(callback), event_adder, dest_file))));
}

}  // namespace update_client
