// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace unzip {

namespace {

class UnzipFilter : public unzip::mojom::UnzipFilter {
 public:
  UnzipFilter(mojo::PendingReceiver<unzip::mojom::UnzipFilter> receiver,
              UnzipFilterCallback filter_callback)
      : receiver_(this, std::move(receiver)),
        filter_callback_(filter_callback) {}

 private:
  // unzip::mojom::UnzipFilter implementation:
  void ShouldUnzipFile(const base::FilePath& path,
                       ShouldUnzipFileCallback callback) override {
    std::move(callback).Run(filter_callback_.Run(path));
  }

  mojo::Receiver<unzip::mojom::UnzipFilter> receiver_;
  UnzipFilterCallback filter_callback_;

  DISALLOW_COPY_AND_ASSIGN(UnzipFilter);
};

class UnzipParams : public base::RefCounted<UnzipParams> {
 public:
  UnzipParams(
      mojo::PendingRemote<mojom::Unzipper> unzipper,
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
      UnzipCallback callback,
      const scoped_refptr<base::SequencedTaskRunner>&
          background_task_runner_keep_alive)
      : unzipper_(std::move(unzipper)),
        callback_task_runner_(callback_task_runner),
        callback_(std::move(callback)),
        background_task_runner_keep_alive_(background_task_runner_keep_alive) {}

  mojo::Remote<mojom::Unzipper>& unzipper() { return unzipper_; }

  void InvokeCallback(bool result) {
    if (callback_) {
      callback_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), result));
    }
  }

  void set_unzip_filter(std::unique_ptr<UnzipFilter> filter) {
    filter_ = std::move(filter);
  }

 private:
  friend class base::RefCounted<UnzipParams>;

  ~UnzipParams() = default;

  // The Remote and UnzipFilter are stored so they do not get deleted before the
  // callback runs.
  mojo::Remote<mojom::Unzipper> unzipper_;
  std::unique_ptr<UnzipFilter> filter_;

  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
  UnzipCallback callback_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(UnzipParams);
};

void UnzipDone(scoped_refptr<UnzipParams> params, bool success) {
  params->InvokeCallback(success);
  params->unzipper().reset();
}

void DoUnzipWithFilter(
    mojo::PendingRemote<mojom::Unzipper> unzipper,
    const base::FilePath& zip_path,
    const base::FilePath& output_dir,
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
    UnzipFilterCallback filter_callback,
    UnzipCallback result_callback,
    const scoped_refptr<base::SequencedTaskRunner>&
        background_task_runner_keep_alive) {
  base::File zip_file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!zip_file.IsValid()) {
    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback), /*result=*/false));
    return;
  }

  mojo::PendingRemote<filesystem::mojom::Directory> directory_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<filesystem::DirectoryImpl>(output_dir, nullptr, nullptr),
      directory_remote.InitWithNewPipeAndPassReceiver());

  // |result_callback| is shared between the connection error handler and the
  // Unzip call using a refcounted UnzipParams object that owns
  // |result_callback|.
  auto unzip_params = base::MakeRefCounted<UnzipParams>(
      std::move(unzipper), callback_task_runner, std::move(result_callback),
      background_task_runner_keep_alive);

  unzip_params->unzipper().set_disconnect_handler(
      base::BindOnce(&UnzipDone, unzip_params, false));

  if (filter_callback.is_null()) {
    unzip_params->unzipper()->Unzip(std::move(zip_file),
                                    std::move(directory_remote),
                                    base::BindOnce(&UnzipDone, unzip_params));
    return;
  }

  mojo::PendingRemote<unzip::mojom::UnzipFilter> unzip_filter_remote;
  unzip_params->set_unzip_filter(std::make_unique<UnzipFilter>(
      unzip_filter_remote.InitWithNewPipeAndPassReceiver(), filter_callback));

  unzip_params->unzipper()->UnzipWithFilter(
      std::move(zip_file), std::move(directory_remote),
      std::move(unzip_filter_remote), base::BindOnce(&UnzipDone, unzip_params));
}

}  // namespace

void Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
           const base::FilePath& zip_path,
           const base::FilePath& output_dir,
           UnzipCallback callback) {
  UnzipWithFilter(std::move(unzipper), zip_path, output_dir,
                  UnzipFilterCallback(), std::move(callback));
}

void UnzipWithFilter(mojo::PendingRemote<mojom::Unzipper> unzipper,
                     const base::FilePath& zip_path,
                     const base::FilePath& output_dir,
                     UnzipFilterCallback filter_callback,
                     UnzipCallback result_callback) {
  DCHECK(!result_callback.is_null());

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
           base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  background_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DoUnzipWithFilter, std::move(unzipper), zip_path,
                     output_dir, base::SequencedTaskRunnerHandle::Get(),
                     filter_callback, std::move(result_callback),
                     background_task_runner));
}

}  // namespace unzip
