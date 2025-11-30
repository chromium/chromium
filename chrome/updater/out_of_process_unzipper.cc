// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/out_of_process_unzipper.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/zlib/google/zip.h"

namespace updater {

namespace {

// A DecodeOperation is a cancellable invocation of DecodeXz.
class DecodeOperation : public base::RefCountedThreadSafe<DecodeOperation> {
 public:
  DecodeOperation(const base::FilePath& out_path,
                  base::OnceCallback<void(bool)> callback)
      : out_path_(out_path), callback_(std::move(callback)) {
    // DecodeOperation is created on one sequence but forever used on another.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // Start must be called only once. May block.
  void Start(UpdaterScope scope, const base::FilePath& in_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::optional<base::FilePath> updater_path =
        GetUpdaterExecutablePath(scope);
    if (!updater_path) {
      Done(false);
      return;
    }
    base::CommandLine command_line(*updater_path);
    command_line.AppendSwitch(kUnzipWorkerSwitch);
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
      Done(false);
      return;
    }
    channel.RemoteProcessLaunchAttempted();
    mojo::ScopedMessagePipeHandle pipe = mojo::OutgoingInvitation::SendIsolated(
        channel.TakeLocalEndpoint(), {}, process.Handle());
    if (!pipe) {
      VLOG(0) << "Failed to send Mojo invitation to the unzipper process.";
      Done(false);
      return;
    }
    mojo::PendingRemote<unzip::mojom::Unzipper> pending_remote(
        std::move(pipe), unzip::mojom::Unzipper::Version_);
    if (!pending_remote) {
      VLOG(0) << "Failed to establish IPC with the unzipper process.";
      Done(false);
      return;
    }
    unzipper_.Bind(std::move(pending_remote));
    unzipper_.set_disconnect_handler(
        base::BindOnce(&DecodeOperation::Done, this, false));
    base::File in(in_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                               base::File::FLAG_WIN_SEQUENTIAL_SCAN |
                               base::File::FLAG_WIN_SHARE_DELETE);
    if (!in.IsValid()) {
      VLOG(1) << "Failed to open input file.";
      Done(false);
      return;
    }
    base::File out(out_path_, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                  base::File::FLAG_WRITE |
                                  base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                  base::File::FLAG_WIN_SEQUENTIAL_SCAN |
                                  base::File::FLAG_WIN_SHARE_DELETE);
    if (!out.IsValid()) {
      VLOG(1) << "Failed to open output file.";
      Done(false);
      return;
    }

    unzipper_->DecodeXz(std::move(in), std::move(out),
                        base::BindOnce(&DecodeOperation::Done, this));
  }

  // Resets the unzipper remote and triggers the completion callback.
  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Done(false);
  }

 private:
  // `Done` may be called multiple times, for example if cancellation is posted
  // to a task runner, but concurrently, the job completes or the remote
  // disconnects.
  void Done(bool result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!result && unzipper_.is_bound()) {
      base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                                 base::GetDeleteFileCallback(out_path_));
    }
    std::move(callback_).Run(result);
    callback_ = base::DoNothing();
    unzipper_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<DecodeOperation>;

  ~DecodeOperation() = default;

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<unzip::mojom::Unzipper> unzipper_;
  const base::FilePath out_path_;
  base::OnceCallback<void(bool)> callback_;
};

}  // namespace

OutOfProcessUnzipper::OutOfProcessUnzipper(UpdaterScope scope)
    : scope_(scope) {}

OutOfProcessUnzipper::~OutOfProcessUnzipper() = default;

void OutOfProcessUnzipper::Unzip(const base::FilePath& zip_path,
                                 const base::FilePath& output_path,
                                 UnzipCompleteCallback callback) {
  // For now, run in-process until the mojo Unzip interface supports symlink
  // preservation and preserving file mtimes on POSIX (crbug.com/40840483).
  std::move(callback).Run(zip::Unzip(zip_path, output_path, {},
  // Allow internal symbolic links in zip files on macOS.
#if BUILDFLAG(IS_POSIX)
                                     zip::UnzipSymlinkOption::PRESERVE
#else
                                     zip::UnzipSymlinkOption::DONT_PRESERVE
#endif
                                     ));
}

base::OnceClosure OutOfProcessUnzipper::DecodeXz(
    const base::FilePath& xz_path,
    const base::FilePath& output_path,
    UnzipCompleteCallback done_callback) {
  auto op = base::MakeRefCounted<DecodeOperation>(output_path,
                                                  std::move(done_callback));
  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(
      FROM_HERE, base::BindOnce(&DecodeOperation::Start, op, scope_, xz_path));
  return base::BindPostTask(runner,
                            base::BindOnce(&DecodeOperation::Cancel, op));
}

OutOfProcessUnzipperFactory::OutOfProcessUnzipperFactory(UpdaterScope scope)
    : scope_(scope) {}

std::unique_ptr<update_client::Unzipper> OutOfProcessUnzipperFactory::Create()
    const {
  return std::make_unique<OutOfProcessUnzipper>(scope_);
}

}  // namespace updater
