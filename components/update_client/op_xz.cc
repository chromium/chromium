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
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/op_zucchini.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client_errors.h"
#include "components/zucchini/zucchini.h"

namespace update_client {

namespace {

void Done(base::OnceCallback<
              void(base::expected<base::FilePath, CategorizedError>)> callback,
          base::RepeatingCallback<void(base::Value::Dict)> event_adder,
          const base::FilePath& out_file,
          bool success) {
  base::Value::Dict event;
  event.Set("eventtype", protocol_request::kEventXz);
  event.Set("eventresult",
            static_cast<int>(success ? protocol_request::kEventResultSuccess
                                     : protocol_request::kEventResultError));
  event_adder.Run(std::move(event));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          [&]() -> base::expected<base::FilePath, CategorizedError> {
            if (success) {
              return out_file;
            }
            return base::unexpected<CategorizedError>(
                {.category = ErrorCategory::kUnpack,
                 .code = static_cast<int>(UnpackerError::kXzFailed)});
          }()));
}

}  // namespace

base::OnceClosure XzOperation(
    std::unique_ptr<Unzipper> unzipper,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const base::FilePath& in_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  base::FilePath dest_file = in_file.DirName().AppendUTF8("decoded_xz");
  Unzipper* unzipper_raw = unzipper.get();
  return unzipper_raw->DecodeXz(
      in_file, dest_file,
      base::BindOnce(
          [](const base::FilePath& in_file, std::unique_ptr<Unzipper> unzipper,
             bool result) {
            base::DeleteFile(in_file);
            return result;
          },
          in_file, std::move(unzipper))
          .Then(base::BindPostTaskToCurrentDefault(base::BindOnce(
              &Done, std::move(callback), event_adder, dest_file))));
}

}  // namespace update_client
