// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_install.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/unpacker.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "third_party/puffin/src/include/puffin/puffpatch.h"

namespace update_client {

namespace {

// The sequence of calls is:
//
// [Original Sequence]      [Blocking Pool]
//
// CrxCache::Put (optional)
// Unpack
// Unpacker::Unpack
// Install
//                          InstallBlocking
//                          installer->Install
// CallbackChecker::Done
//                          [lambda to delete unpack path]
// InstallComplete
// [original callback]

// CallbackChecker ensures that a progress callback is not posted after a
// completion callback. It is only accessed and modified on the main sequence.
// Both callbacks maintain a reference to an instance of this class.
class CallbackChecker : public base::RefCountedThreadSafe<CallbackChecker> {
 public:
  CallbackChecker(
      base::OnceCallback<void(const CrxInstaller::Result&)> callback,
      CrxInstaller::ProgressCallback progress_callback)
      : callback_(std::move(callback)), progress_callback_(progress_callback) {}
  CallbackChecker(const CallbackChecker&) = delete;
  CallbackChecker& operator=(const CallbackChecker&) = delete;

  void Progress(int progress) { progress_callback_.Run(progress); }

  void Done(const CrxInstaller::Result& result) {
    progress_callback_ = base::DoNothing();
    std::move(callback_).Run(result);
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackChecker>;
  ~CallbackChecker() = default;
  base::OnceCallback<void(const CrxInstaller::Result&)> callback_;
  CrxInstaller::ProgressCallback progress_callback_;
};

// Runs on the original sequence.
void InstallComplete(
    base::OnceCallback<void(const CrxInstaller::Result&)> callback,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const CrxInstaller::Result& result) {
  // TODO(crbug.com/353249967): Add an event describing the install's outcome.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

// Runs in the blocking thread pool.
void InstallBlocking(
    CrxInstaller::ProgressCallback progress_callback,
    base::OnceCallback<void(const CrxInstaller::Result&)> callback,
    const base::FilePath& unpack_path,
    const std::string& public_key,
    const std::string& next_fp,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    scoped_refptr<CrxInstaller> installer) {
  // Write manifest.fingerprint.
  if (!base::WriteFile(
          unpack_path.Append(FILE_PATH_LITERAL("manifest.fingerprint")),
          next_fp)) {
    std::move(callback).Run(CrxInstaller::Result(
        {.category_ = ErrorCategory::kInstall,
         .code_ = static_cast<int>(InstallError::FINGERPRINT_WRITE_FAILED),
         .extra_ = static_cast<int>(logging::GetLastSystemErrorCode())}));
    return;
  }

  installer->Install(unpack_path, public_key, std::move(install_params),
                     progress_callback, std::move(callback));
}

// Runs on the original sequence.
void Install(base::OnceCallback<void(const CrxInstaller::Result&)> callback,
             const std::string& next_fp,
             std::unique_ptr<CrxInstaller::InstallParams> install_params,
             scoped_refptr<CrxInstaller> installer,
             CrxInstaller::ProgressCallback progress_callback,
             const Unpacker::Result& result) {
  if (result.error != UnpackerError::kNone) {
    std::move(callback).Run(
        CrxInstaller::Result({.category_ = ErrorCategory::kUnpack,
                              .code_ = static_cast<int>(result.error),
                              .extra_ = result.extended_error}));
    return;
  }

  progress_callback.Run(-1);

  // Prepare the callbacks. Delete unpack_path when the completion
  // callback is called.
  auto checker = base::MakeRefCounted<CallbackChecker>(
      base::BindOnce(
          [](base::OnceCallback<void(const CrxInstaller::Result&)> callback,
             const base::FilePath& unpack_path,
             const CrxInstaller::Result& result) {
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE, kTaskTraits,
                base::BindOnce(IgnoreResult(&base::DeletePathRecursively),
                               unpack_path),
                base::BindOnce(std::move(callback), result));
          },
          std::move(callback), result.unpack_path),
      progress_callback);

  // Run installer.
  base::ThreadPool::PostTask(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&InstallBlocking,
                     base::BindPostTaskToCurrentDefault(base::BindRepeating(
                         &CallbackChecker::Progress, checker)),
                     base::BindPostTaskToCurrentDefault(
                         base::BindOnce(&CallbackChecker::Done, checker)),
                     result.unpack_path, result.public_key, next_fp,
                     std::move(install_params), installer));
}

// Runs on the original sequence.
void Unpack(base::OnceCallback<void(const Unpacker::Result&)> callback,
            const base::FilePath& crx_file,
            std::unique_ptr<Unzipper> unzipper,
            const std::vector<uint8_t>& pk_hash,
            crx_file::VerifierFormat crx_format,
            base::expected<base::FilePath, UnpackerError> cache_result) {
  if (!cache_result.has_value()) {
    // Caching is optional: continue with the install, but add a task to clean
    // up crx_file.
    callback = base::BindOnce(
        [](const base::FilePath& crx_file,
           base::OnceCallback<void(const Unpacker::Result&)> callback,
           const Unpacker::Result& result) {
          base::ThreadPool::PostTaskAndReply(
              FROM_HERE, kTaskTraits,
              base::BindOnce(IgnoreResult(&base::DeleteFile), crx_file),
              base::BindOnce(std::move(callback), result));
        },
        crx_file, std::move(callback));
  }

  // Unpack the file.
  base::ThreadPool::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &Unpacker::Unpack, pk_hash,
              // If and only if cached, the original path no longer exists.
              cache_result.has_value() ? cache_result.value() : crx_file,
              std::move(unzipper), crx_format,
              base::BindPostTaskToCurrentDefault(std::move(callback))));
}

}  // namespace

base::OnceClosure InstallOperation(
    scoped_refptr<CrxCache> crx_cache,
    std::unique_ptr<Unzipper> unzipper,
    crx_file::VerifierFormat crx_format,
    const std::string& id,
    const std::vector<uint8_t>& pk_hash,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    const std::string& next_fp,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CrxInstaller::ProgressCallback progress_callback,
    base::OnceCallback<void(const CrxInstaller::Result&)> callback,
    const base::FilePath& crx_file) {
  crx_cache->Put(
      crx_file, id, next_fp,
      base::BindOnce(
          &Unpack,
          base::BindOnce(&Install,
                         base::BindOnce(&InstallComplete, std::move(callback),
                                        event_adder),
                         next_fp, std::move(install_params), installer,
                         progress_callback),
          crx_file, std::move(unzipper), pk_hash, crx_format));
  return base::DoNothing();
}

}  // namespace update_client
