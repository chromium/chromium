// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/pipeline.h"

#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/action_runner.h"
#include "components/update_client/cancellation.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/op_download.h"
#include "components/update_client/op_install.h"
#include "components/update_client/op_puffin.h"
#include "components/update_client/op_xz.h"
#include "components/update_client/op_zucchini.h"
#include "components/update_client/pipeline_util.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_engine.h"

namespace update_client {

namespace {

// A `Pipeline` represents an ordered sequence of operations that the update
// server has instructed the client to perform. Operations are functions with
// two inputs:
//   1. A `FilePath`.
//   2. A completion callback, which takes a `FilePath` or error. Operations
//      that don't generate a file (e.g. a "run" action) propagate their input
//      path.
// Operations return a cancellation callback when run. Operations that can't
// be cancelled return `base::DoNothing` as their cancellation callback.
// The first operation in each pipeline must tolerate an empty FilePath as
// input.
using Operation = base::OnceCallback<base::OnceClosure(
    const base::FilePath&,
    base::OnceCallback<void(
        base::expected<base::FilePath, CategorizedError>)>)>;

constexpr CategorizedError kUnsupportedOperationError = CategorizedError(
    {.category = ErrorCategory::kUpdateCheck,
     .code = static_cast<int>(ProtocolError::UNSUPPORTED_OPERATION)});

constexpr CategorizedError kInvalidOperationAttributesError = CategorizedError(
    {.category = ErrorCategory::kUpdateCheck,
     .code = static_cast<int>(ProtocolError::INVALID_OPERATION_ATTRIBUTES)});

// `Pipeline` manages the flow of operations, passing the output path of
// each operation to the next one, short-circuiting on errors.
//
// Additionally, if a pipeline fails, it may fall back to a backup pipeline.
//
// `Pipeline` is refcounted. The callback passed to the currently-running
// operation holds the single ref to the pipeline, keeping it alive until
// `OpComplete` returns (after which, if there is another operation, another
// completion callback will keep it alive).
class Pipeline : public base::RefCountedThreadSafe<Pipeline> {
 public:
  Pipeline(
      std::queue<Operation> operations,
      scoped_refptr<Pipeline> fallback);
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  // Each `Pipeline` can only be used once. Returns a cancellation callback.
  base::OnceClosure Start(
      base::OnceCallback<void(const CategorizedError&)> callback);

 private:
  friend class base::RefCountedThreadSafe<Pipeline>;
  virtual ~Pipeline() = default;

  void StartNext(const base::FilePath& path);
  void OpComplete(base::expected<base::FilePath, CategorizedError>);

  SEQUENCE_CHECKER(sequence_checker_);
  std::queue<Operation> operations_;
  scoped_refptr<Pipeline> fallback_;
  scoped_refptr<Cancellation> cancel_ = base::MakeRefCounted<Cancellation>();
  base::OnceCallback<void(const CategorizedError&)> callback_;
};

Pipeline::Pipeline(std::queue<Operation> operations,
                   scoped_refptr<Pipeline> fallback)
    : operations_(std::move(operations)), fallback_(fallback) {}

base::OnceClosure Pipeline::Start(
    base::OnceCallback<void(const CategorizedError&)> callback) {
  CHECK(!callback_);
  callback_ = std::move(callback);
  StartNext({});
  return base::BindOnce(&Cancellation::Cancel, cancel_);
}

void Pipeline::StartNext(const base::FilePath& path) {
  Operation next = std::move(operations_.front());
  operations_.pop();
  cancel_->OnCancel(
      std::move(next).Run(path, base::BindOnce(&Pipeline::OpComplete, this)));
}

void Pipeline::OpComplete(
    base::expected<base::FilePath, CategorizedError> result) {
  cancel_->Clear();
  if (!result.has_value()) {
    if (fallback_ && !cancel_->IsCancelled()) {
      // Pipeline failed, fall back to next pipeline.
      cancel_->OnCancel(fallback_->Start(std::move(callback_)));
      return;
    }
    // Pipeline failed and fallbacks exhausted or cancelled.
    std::move(callback_).Run(result.error());
    return;
  }
  if (operations_.empty()) {
    // Pipeline successfully completed.
    std::move(callback_).Run({});
    return;
  }
  if (cancel_->IsCancelled()) {
    // Pipeline still running, but cancelled.
    std::move(callback_).Run(
        {.category = ErrorCategory::kService,
         .code = static_cast<int>(ServiceError::CANCELLED)});
    return;
  }
  // Pipeline still running: start next operation.
  StartNext(result.value());
}

// RunOperation adapts RunAction to an Operation-friendly signature. The
// operation ignores the outcome of the RunAction (RunAction failures should not
// stop the pipeline) and passes the previous operation's file onward to the
// next operation.
base::OnceClosure RunOperation(
    scoped_refptr<ActionHandler> handler,
    scoped_refptr<CrxInstaller> installer,
    const std::string& file,
    const std::string& session_id,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    const base::FilePath& previous_operation_output,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  return RunAction(
      handler, installer, file, session_id, event_adder, state_tracker,
      base::BindOnce(
          [](base::OnceCallback<void(
                 base::expected<base::FilePath, CategorizedError>)> callback,
             const base::FilePath& previous_operation_output, bool success,
             int error,
             int extra) { std::move(callback).Run(previous_operation_output); },
          std::move(callback), previous_operation_output));
}

// Wraps an operation in a check of the CRX Cache, skipping it if the associated
// hash is already cached.
Operation SkipIfCached(
    base::RepeatingCallback<
        void(base::OnceCallback<void(
                 base::expected<base::FilePath, UnpackerError>)>)> cache_getter,
    Operation operation) {
  return base::BindOnce(
      [](base::RepeatingCallback<void(
             base::OnceCallback<void(
                 base::expected<base::FilePath, UnpackerError>)>)> cache_getter,
         Operation operation, const base::FilePath& path_in,
         base::OnceCallback<void(
             base::expected<base::FilePath, CategorizedError>)> callback) {
        auto cancellation = base::MakeRefCounted<Cancellation>();
        cache_getter.Run(base::BindOnce(
            [](scoped_refptr<Cancellation> cancellation, Operation operation,
               const base::FilePath& path_in,
               base::OnceCallback<void(
                   base::expected<base::FilePath, CategorizedError>)> callback,
               base::expected<base::FilePath, UnpackerError> cached_path) {
              if (cached_path.has_value()) {
                // Skip the operation, and return the path to the next step.
                std::move(callback).Run(cached_path.value());
                return;
              }
              // Else, run the task and bind its cancellation callback to the
              // cancellation returned by the wrapper.
              cancellation->OnCancel(
                  std::move(operation).Run(path_in, std::move(callback)));
            },
            cancellation, std::move(operation), path_in, std::move(callback)));
        return base::BindOnce(&Cancellation::Cancel, cancellation);
      },
      cache_getter, std::move(operation));
}

// Creates an operation queue to replace the existing queue that always fails
// for cases where a pipeline is impossible to process.
std::queue<Operation> MakeErrorOperations(
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CategorizedError error,
    const int event_type) {
  std::queue<Operation> error_ops;
  error_ops.push(base::BindOnce(
      [](base::RepeatingCallback<void(base::Value::Dict)> event_adder,
         CategorizedError error, const int event_type, const base::FilePath&,
         base::OnceCallback<void(
             base::expected<base::FilePath, CategorizedError>)> callback)
          -> base::OnceClosure {
        event_adder.Run(MakeSimpleOperationEvent(
            kInvalidOperationAttributesError, event_type));
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           base::unexpected(kInvalidOperationAttributesError)));
        return base::DoNothing();
      },
      event_adder, error, event_type));
  return error_ops;
}

std::queue<Operation> MakeOperations(
    scoped_refptr<Configurator> config,
    base::RepeatingCallback<int64_t(const base::FilePath&)> get_available_space,
    bool is_foreground,
    const std::string& session_id,
    scoped_refptr<CrxCache> crx_cache,
    crx_file::VerifierFormat crx_format,
    const std::string& id,
    const std::vector<uint8_t>& pk_hash,
    const std::string& install_data_index,
    scoped_refptr<CrxInstaller> installer,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CrxDownloader::ProgressCallback download_progress_callback,
    CrxInstaller::ProgressCallback install_progress_callback,
    base::RepeatingCallback<void(const CrxInstaller::Result&)>
        install_complete_callback,
    scoped_refptr<ActionHandler> action_handler,
    const ProtocolParser::Pipeline& pipeline,
    base::RepeatingCallback<
        void(base::OnceCallback<
             void(base::expected<base::FilePath, UnpackerError>)>)> cache_check,
    const std::string& install_data) {
  std::queue<Operation> ops;
  for (const ProtocolParser::Operation& operation : pipeline.operations) {
    if (operation.type == "download") {
      // expects: `urls` (list of url objects), `size`, and `out` (hash object)
      if (operation.urls.empty() || operation.size <= 0 ||
          operation.sha256_out.empty()) {
        return MakeErrorOperations(event_adder,
                                   kInvalidOperationAttributesError,
                                   protocol_request::kEventDownload);
      }
      ops.push(SkipIfCached(
          cache_check,
          base::BindOnce(&DownloadOperation, config, id, get_available_space,
                         is_foreground, operation.urls, operation.size,
                         operation.sha256_out, event_adder, state_tracker,
                         download_progress_callback)));
    } else if (operation.type == "puff") {
      // expects: `previous` (hash object) and `out` (hash object)
      if (operation.sha256_previous.empty() || operation.sha256_out.empty()) {
        return MakeErrorOperations(event_adder,
                                   kInvalidOperationAttributesError,
                                   protocol_request::kEventPuff);
      }
      ops.push(SkipIfCached(
          cache_check,
          base::BindOnce(&PuffOperation, crx_cache,
                         config->GetPatcherFactory()->Create(), event_adder,
                         state_tracker, operation.sha256_previous,
                         operation.sha256_out)));
    } else if (operation.type == "xz") {
      // expects no extra fields.
      ops.push(SkipIfCached(
          cache_check,
          base::BindOnce(&XzOperation, config->GetUnzipperFactory()->Create(),
                         event_adder, state_tracker)));
    } else if (operation.type == "zucc") {
      // expects: `previous` (hash object) and `out` (hash object)
      if (operation.sha256_previous.empty() || operation.sha256_out.empty()) {
        return MakeErrorOperations(event_adder,
                                   kInvalidOperationAttributesError,
                                   protocol_request::kEventZucchini);
      }
      ops.push(SkipIfCached(
          cache_check,
          base::BindOnce(&ZucchiniOperation, crx_cache,
                         config->GetPatcherFactory()->Create(), event_adder,
                         state_tracker, operation.sha256_previous,
                         operation.sha256_out)));
    } else if (operation.type == "crx3") {
      // expects: `in` (hash object)
      // Note: `path` and `arguments` fields are optional.
      if (operation.sha256_in.empty()) {
        return MakeErrorOperations(event_adder,
                                   kInvalidOperationAttributesError,
                                   protocol_request::kEventCrx3);
      }
      ops.push(base::BindOnce(
          &InstallOperation, crx_cache, config->GetUnzipperFactory()->Create(),
          crx_format, id, config->GetProdId(), operation.sha256_in, pk_hash,
          installer,
          operation.path.empty()
              ? nullptr
              : std::make_unique<CrxInstaller::InstallParams>(
                    operation.path, operation.arguments, install_data),
          event_adder, state_tracker, install_progress_callback,
          install_complete_callback));
    } else if (operation.type == "run") {
      // expects: `path`
      // Note: `arguments` field is optional.
      if (operation.path.empty()) {
        return MakeErrorOperations(event_adder,
                                   kInvalidOperationAttributesError,
                                   protocol_request::kEventAction);
      }
      ops.push(base::BindOnce(&RunOperation, action_handler, installer,
                              operation.path, session_id, event_adder,
                              state_tracker));
    } else {
      // A compliant server shouldn't serve an operation type that was not in
      // the acceptformat, but if it does, or if the client has a bug, replace
      // the entire pipeline with an operation that simply records an error.
      VLOG(2) << "Unrecognized operation '" << operation.type
              << "', skipping pipeline.";
      return MakeErrorOperations(event_adder, kUnsupportedOperationError,
                                 protocol_request::kEventUnknown);
    }
  }
  return ops;
}

}  // namespace

void MakePipeline(
    scoped_refptr<Configurator> config,
    base::RepeatingCallback<int64_t(const base::FilePath&)> get_available_space,
    bool is_foreground,
    const std::string& session_id,
    scoped_refptr<CrxCache> crx_cache,
    crx_file::VerifierFormat crx_format,
    const std::string& id,
    const std::vector<uint8_t>& pk_hash,
    const std::string& install_data_index,
    scoped_refptr<CrxInstaller> installer,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CrxDownloader::ProgressCallback download_progress_callback,
    CrxInstaller::ProgressCallback install_progress_callback,
    base::RepeatingCallback<void(const CrxInstaller::Result&)>
        install_complete_callback,
    scoped_refptr<ActionHandler> action_handler,
    const ProtocolParser::App& result,
    base::OnceCallback<void(
        base::expected<base::OnceCallback<base::OnceClosure(
                           base::OnceCallback<void(const CategorizedError&)>)>,
                       CategorizedError>)> callback) {
  if (result.status == "noupdate") {
    // With a noupdate, there's nothing to do.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::unexpected(CategorizedError())));
    return;
  }

  if (result.status != "ok") {
    // Any error status should just result in a kUpdateCheck error.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(CategorizedError(
                           {.category = ErrorCategory::kUpdateCheck}))));
    return;
  }

  std::string install_data = [&]() -> std::string {
    if (install_data_index.empty() || result.data.empty()) {
      return "";
    }
    const auto it =
        std::ranges::find(result.data, install_data_index,
                          &ProtocolParser::Data::install_data_index);
    return it != std::end(result.data) ? it->text : "";
  }();

  // Assemble the pipelines from last to first.
  scoped_refptr<Pipeline> fallback = nullptr;
  for (const ProtocolParser::Pipeline& pipeline :
       base::Reversed(result.pipelines)) {
    // First, scan the operations to find any to-be-installed CRX so that
    // downloads can be skipped if it is already in cache.
    auto cache_check = base::BindRepeating(
        [](base::OnceCallback<void(
               base::expected<base::FilePath, UnpackerError>)> callback) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  std::move(callback),
                  base::unexpected(UnpackerError::kCrxCacheFileNotCached)));
        });
    for (const ProtocolParser::Operation& operation : pipeline.operations) {
      if (operation.type == "crx3") {
        cache_check = base::BindRepeating(&CrxCache::GetByHash, crx_cache,
                                          operation.sha256_in);
        break;
      }
    }

    // Then, assemble the pipeline. If the pipeline fails, it falls back to the
    // previously-assembled pipeline.
    fallback = base::MakeRefCounted<Pipeline>(
        MakeOperations(
            config, get_available_space, is_foreground, session_id, crx_cache,
            crx_format, id, pk_hash, install_data_index, installer,
            state_tracker,
            base::BindRepeating(
                [](base::RepeatingCallback<void(base::Value::Dict)> event_adder,
                   const std::string& pipeline_id, base::Value::Dict event) {
                  event.Set("pipeline_id", pipeline_id);
                  event_adder.Run(std::move(event));
                },
                event_adder, pipeline.pipeline_id),
            download_progress_callback, install_progress_callback,
            install_complete_callback, action_handler, pipeline, cache_check,
            install_data),
        fallback);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                base::BindOnce(&Pipeline::Start, fallback)));
}

}  // namespace update_client
