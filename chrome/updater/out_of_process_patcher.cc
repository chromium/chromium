// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/out_of_process_patcher.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/zlib/google/zip.h"

namespace updater {

namespace {

// A PatchOperation is an invocation of puffin or zucchini patching.
class PatchOperation : public base::RefCountedThreadSafe<PatchOperation> {
 public:
  explicit PatchOperation(base::OnceCallback<void(int)> callback)
      : callback_(std::move(callback)) {}

  // Start the helper process and return true if successful.
  bool StartHelper(UpdaterScope scope) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::optional<base::FilePath> updater_path =
        GetUpdaterExecutablePath(scope);
    if (!updater_path) {
      Done(kErrorGettingUpdaterPath);
      return false;
    }
    base::CommandLine command_line(*updater_path);
    command_line.AppendSwitch(kPatchWorkerSwitch);
    if (IsSystemInstall(scope)) {
      command_line.AppendSwitch(kSystemSwitch);
    }

    base::LaunchOptions options;
    mojo::PlatformChannel channel;
    channel.PrepareToPassRemoteEndpoint(&options, &command_line);
    base::Process process = base::LaunchProcess(command_line, options);
    VLOG(2) << "Launching " << command_line.GetArgumentsString();
    if (!process.IsValid()) {
      VLOG(2) << "Failure.";
      Done(kErrorLaunchingProcess);
      return false;
    }
    channel.RemoteProcessLaunchAttempted();
    mojo::ScopedMessagePipeHandle pipe = mojo::OutgoingInvitation::SendIsolated(
        channel.TakeLocalEndpoint(), {}, process.Handle());
    if (!pipe) {
      VLOG(0) << "Failed to send Mojo invitation to the patcher process.";
      Done(kErrorMojoConnectionFailure);
      return false;
    }
    mojo::PendingRemote<patch::mojom::FilePatcher> pending_remote(
        std::move(pipe), patch::mojom::FilePatcher::Version_);
    if (!pending_remote) {
      VLOG(0) << "Failed to establish IPC with the patcher process.";
      Done(kErrorMojoConnectionFailure);
      return false;
    }
    patcher_.Bind(std::move(pending_remote));
    patcher_.set_disconnect_handler(
        base::BindOnce(&PatchOperation::Done, this, kErrorIpcDisconnect));
    return true;
  }

  void PatchZucchini(UpdaterScope scope,
                     base::File old,
                     base::File patch,
                     base::File out) {
    if (!StartHelper(scope)) {
      return;
    }
    patcher_->PatchFileZucchini(
        std::move(old), std::move(patch), std::move(out),
        base::BindOnce([](zucchini::status::Code status) {
          return static_cast<int>(status);
        }).Then(base::BindOnce(&PatchOperation::Done, this)));
  }

  void PatchPuffPatch(UpdaterScope scope,
                      base::File old,
                      base::File patch,
                      base::File out) {
    if (!StartHelper(scope)) {
      return;
    }
    patcher_->PatchFilePuffPatch(std::move(old), std::move(patch),
                                 std::move(out),
                                 base::BindOnce(&PatchOperation::Done, this));
  }

 private:
  void Done(int result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback_).Run(result);
    callback_ = base::DoNothing();
    patcher_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<PatchOperation>;

  ~PatchOperation() = default;

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<patch::mojom::FilePatcher> patcher_;
  base::OnceCallback<void(int)> callback_;
};

class OutOfProcessPatcher : public update_client::Patcher {
 public:
  explicit OutOfProcessPatcher(UpdaterScope scope) : scope_(scope) {}

  OutOfProcessPatcher(const OutOfProcessPatcher&) = delete;
  OutOfProcessPatcher& operator=(const OutOfProcessPatcher&) = delete;

  // Overrides for update_client::Patcher.
  void PatchPuffPatch(base::File old_file,
                      base::File patch_file,
                      base::File destination_file,
                      PatchCompleteCallback callback) const override {
    base::MakeRefCounted<PatchOperation>(std::move(callback))
        ->PatchPuffPatch(scope_, std::move(old_file), std::move(patch_file),
                         std::move(destination_file));
  }

  void PatchZucchini(base::File old_file,
                     base::File patch_file,
                     base::File destination_file,
                     PatchCompleteCallback callback) const override {
    base::MakeRefCounted<PatchOperation>(std::move(callback))
        ->PatchZucchini(scope_, std::move(old_file), std::move(patch_file),
                        std::move(destination_file));
  }

 private:
  ~OutOfProcessPatcher() override = default;

  UpdaterScope scope_;
};

}  // namespace

OutOfProcessPatcherFactory::OutOfProcessPatcherFactory(UpdaterScope scope)
    : scope_(scope) {}

scoped_refptr<update_client::Patcher> OutOfProcessPatcherFactory::Create()
    const {
  return base::MakeRefCounted<OutOfProcessPatcher>(scope_);
}

}  // namespace updater
