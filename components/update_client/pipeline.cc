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
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
using RepeatingOperation = base::RepeatingCallback<base::OnceClosure(
    const base::FilePath&,
    base::OnceCallback<void(
        base::expected<base::FilePath, CategorizedError>)>)>;

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
      const base::FilePath& first_path,
      base::RepeatingCallback<void(const CategorizedError&)> outcome_recorder,
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
  base::FilePath first_path_;
  base::RepeatingCallback<void(const CategorizedError&)> outcome_recorder_;
  scoped_refptr<Pipeline> fallback_;
  scoped_refptr<Cancellation> cancel_ = base::MakeRefCounted<Cancellation>();
  base::OnceCallback<void(const CategorizedError&)> callback_;
};

Pipeline::Pipeline(
    std::queue<Operation> operations,
    const base::FilePath& first_path,
    base::RepeatingCallback<void(const CategorizedError&)> outcome_recorder,
    scoped_refptr<Pipeline> fallback)
    : operations_(std::move(operations)),
      first_path_(first_path),
      outcome_recorder_(outcome_recorder),
      fallback_(fallback) {}

base::OnceClosure Pipeline::Start(
    base::OnceCallback<void(const CategorizedError&)> callback) {
  CHECK(!callback_);
  callback_ = std::move(callback);
  StartNext(first_path_);
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
    outcome_recorder_.Run(result.error());
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
        {.category_ = ErrorCategory::kService,
         .code_ = static_cast<int>(ServiceError::CANCELLED)});
    return;
  }
  // Pipeline still running: start next operation.
  StartNext(result.value());
}

void AssemblePipeline(
    std::optional<Operation> download_diff,
    std::optional<Operation> patch_diff,
    Operation download_full,
    RepeatingOperation install,
    std::optional<RepeatingOperation> run,
    base::RepeatingCallback<void(const CategorizedError&)>
        diff_outcome_recorder,
    base::OnceCallback<void(
        base::expected<base::OnceCallback<base::OnceClosure(
                           base::OnceCallback<void(const CategorizedError&)>)>,
                       CategorizedError>)> callback,
    base::expected<base::FilePath, UnpackerError> cached_installer,
    base::expected<base::FilePath, UnpackerError> prev_installer) {
  if (cached_installer.has_value()) {
    // Skip downloading and patching; run the cached installer. No fallbacks.
    std::move(callback).Run(base::BindOnce(
        &Pipeline::Start,
        base::MakeRefCounted<Pipeline>(
            [&] {
              std::queue<Operation> ops;
              ops.push(base::BindOnce(install));
              if (run) {
                ops.push(base::BindOnce(*run));
              }
              return ops;
            }(),
            cached_installer.value(), base::DoNothing(), nullptr)));
    return;
  }

  scoped_refptr<Pipeline> full = nullptr;
  {
    // Construct the full update pipeline.
    // TODO(crbug.com/353249967): It's possible for the diff pipeline to have
    // created and cached the full download output. Adjust the download step to
    // check the cache.
    std::queue<Operation> ops;
    ops.push(std::move(download_full));
    ops.push(base::BindOnce(install));
    if (run) {
      ops.push(base::BindOnce(*run));
    }
    full = base::MakeRefCounted<Pipeline>(std::move(ops), base::FilePath(),
                                          base::DoNothing(), full);
  }

  if (download_diff && prev_installer.has_value()) {
    // Do a differential update that falls back to a full update.
    CHECK(patch_diff);
    std::queue<Operation> ops;
    ops.push(std::move(*download_diff));
    ops.push(std::move(*patch_diff));
    ops.push(base::BindOnce(install));
    if (run) {
      ops.push(base::BindOnce(*run));
    }
    std::move(callback).Run(base::BindOnce(
        &Pipeline::Start,
        base::MakeRefCounted<Pipeline>(std::move(ops), base::FilePath(),
                                       diff_outcome_recorder, full)));
    return;
  }

  // Else, full update only.
  std::move(callback).Run(base::BindOnce(&Pipeline::Start, full));
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
    const std::string& prev_fp,
    scoped_refptr<CrxInstaller> installer,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CrxDownloader::ProgressCallback download_progress_callback,
    CrxInstaller::ProgressCallback install_progress_callback,
    base::RepeatingCallback<void(const CrxInstaller::Result&)>
        install_complete_callback,
    scoped_refptr<ActionHandler> action_handler,
    base::RepeatingCallback<void(const CategorizedError&)>
        diff_outcome_recorder,
    const ProtocolParser::Result& result,
    base::OnceCallback<void(
        base::expected<base::OnceCallback<base::OnceClosure(
                           base::OnceCallback<void(const CategorizedError&)>)>,
                       CategorizedError>)> callback) {
  // Run action.
  std::optional<RepeatingOperation> run;
  if (!result.action_run.empty()) {
    run = base::BindRepeating(
              [](base::RepeatingCallback<void(ComponentState)> state_tracker,
                 const base::FilePath& file,
                 base::OnceCallback<void(
                     base::expected<base::FilePath, CategorizedError>)>
                     callback) {
                // Discard the input file path, and adapt callback.
                state_tracker.Run(ComponentState::kRun);
                return base::BindOnce(
                           [](const base::FilePath& file, bool success,
                              int error, int extra) {
                             // Discard any error result: RunAction failures
                             // don't end the pipeline.
                             return file;
                           },
                           file)
                    .Then(std::move(callback));
              },
              state_tracker)
              .Then(base::BindRepeating(&RunAction, action_handler, installer,
                                        result.action_run, session_id,
                                        event_adder));
  }

  if (result.status == "noupdate") {
    if (run) {
      // The pipeline has a run action only.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         base::BindOnce(&Pipeline::Start,
                                        base::MakeRefCounted<Pipeline>(
                                            [&] {
                                              std::queue<Operation> ops;
                                              ops.push(*run);
                                              return ops;
                                            }(),
                                            base::FilePath(),
                                            diff_outcome_recorder, nullptr))));
      return;
    }
    // Else, with a noupdate, there's nothing to do.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::unexpected(CategorizedError())));
    return;
  }

  if (result.status != "ok" || result.manifest.packages.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(CategorizedError(
                           {.category_ = ErrorCategory::kUpdateCheck}))));
    return;
  }

  // Download the update.
  Operation download_full =
      base::BindOnce(
          [](base::RepeatingCallback<void(ComponentState)> state_tracker,
             const base::FilePath&,
             base::OnceCallback<void(
                 base::expected<base::FilePath, CategorizedError>)> callback) {
            // Discard the input file path, set state to downloading, pass
            // `callback` on to the download.
            state_tracker.Run(ComponentState::kDownloading);
            return callback;
          },
          state_tracker)
          .Then(base::BindOnce(
              &DownloadOperation, config, get_available_space, is_foreground,
              [&] {
                std::vector<GURL> urls;
                for (const auto& base_url : result.crx_urls) {
                  const GURL url =
                      base_url.Resolve(result.manifest.packages[0].name);
                  if (url.is_valid()) {
                    urls.push_back(url);
                  }
                }
                return urls;
              }(),
              result.manifest.packages[0].size,
              result.manifest.packages[0].hash_sha256, event_adder,
              download_progress_callback));

  // Download and patch the diff update.
  std::optional<Operation> download_diff;
  std::optional<Operation> patch_diff;
  if (!result.manifest.packages[0].hashdiff_sha256.empty()) {
    download_diff =
        base::BindOnce(
            [](base::RepeatingCallback<void(ComponentState)> state_tracker,
               const base::FilePath&,
               base::OnceCallback<void(
                   base::expected<base::FilePath, CategorizedError>)>
                   callback) {
              // Discard the input file path, pass `callback` on to the
              // download.
              state_tracker.Run(ComponentState::kDownloading);
              return callback;
            },
            state_tracker)
            .Then(base::BindOnce(
                &DownloadOperation, config, get_available_space, is_foreground,
                [&] {
                  std::vector<GURL> urls;
                  for (const auto& base_url : result.crx_diffurls) {
                    const GURL url =
                        base_url.Resolve(result.manifest.packages[0].namediff);
                    if (url.is_valid()) {
                      urls.push_back(url);
                    }
                  }
                  return urls;
                }(),
                result.manifest.packages[0].sizediff,
                result.manifest.packages[0].hashdiff_sha256, event_adder,
                download_progress_callback));
    patch_diff = base::BindOnce(&PuffOperation, crx_cache,
                                config->GetPatcherFactory()->Create(),
                                event_adder, id, prev_fp);
  }

  // Install the update.
  RepeatingOperation install = base::BindRepeating(
      [](scoped_refptr<Configurator> config, scoped_refptr<CrxCache> crx_cache,
         crx_file::VerifierFormat crx_format, const std::string& id,
         const std::vector<uint8_t>& pk_hash,
         scoped_refptr<CrxInstaller> installer, const std::string& run,
         const std::string& arguments, const std::string& install_data,
         const std::string& fingerprint,
         base::RepeatingCallback<void(ComponentState)> state_tracker,
         base::RepeatingCallback<void(base::Value::Dict)> event_adder,
         CrxInstaller::ProgressCallback install_progress_callback,
         base::RepeatingCallback<void(const CrxInstaller::Result&)>
             install_complete_callback,
         const base::FilePath& file,
         base::OnceCallback<void(
             base::expected<base::FilePath, CategorizedError>)> callback) {
        state_tracker.Run(ComponentState::kUpdating);
        return InstallOperation(
            crx_cache, config->GetUnzipperFactory()->Create(), crx_format, id,
            pk_hash, installer,
            run.empty() ? nullptr
                        : std::make_unique<CrxInstaller::InstallParams>(
                              run, arguments, install_data),
            fingerprint, event_adder, install_progress_callback,
            base::BindOnce(
                [](const base::FilePath& file,
                   base::RepeatingCallback<void(const CrxInstaller::Result&)>
                       install_complete_callback,
                   const CrxInstaller::Result& result)
                    -> base::expected<base::FilePath, CategorizedError> {
                  install_complete_callback.Run(result);
                  if (result.result.category_ != ErrorCategory::kNone) {
                    return base::unexpected(result.result);
                  }
                  return file;
                },
                file, install_complete_callback)
                .Then(std::move(callback)),
            file);
      },
      config, crx_cache, crx_format, id, pk_hash, installer,
      result.manifest.run, result.manifest.arguments,
      [&]() -> std::string {
        if (install_data_index.empty() || result.data.empty()) {
          return "";
        }
        const auto it = base::ranges::find(
            result.data, install_data_index,
            &ProtocolParser::Result::Data::install_data_index);
        return it != std::end(result.data) ? it->text : "";
      }(),
      result.manifest.packages[0].fingerprint, state_tracker, event_adder,
      install_progress_callback, install_complete_callback);

  crx_cache->Get(
      id, result.manifest.packages[0].fingerprint,
      base::BindOnce(
          [](scoped_refptr<CrxCache> crx_cache, const std::string& id,
             const std::string& prev_fp,
             base::OnceCallback<void(
                 base::expected<base::FilePath, UnpackerError>,
                 base::expected<base::FilePath, UnpackerError>)> callback,
             base::expected<base::FilePath, UnpackerError> installer) {
            crx_cache->Get(id, prev_fp,
                           base::BindOnce(std::move(callback), installer));
          },
          crx_cache, id, prev_fp,
          base::BindOnce(&AssemblePipeline, std::move(download_diff),
                         std::move(patch_diff), std::move(download_full),
                         install, run, diff_outcome_recorder,
                         std::move(callback))));
  return;
}

}  // namespace update_client
