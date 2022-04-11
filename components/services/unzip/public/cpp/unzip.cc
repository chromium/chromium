// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace unzip {
namespace {

std::string Redact(const base::FilePath& path) {
  return LOG_IS_ON(INFO) ? "'" + path.AsUTF8Unsafe() + "'" : "(redacted)";
}

class UnzipParams : public base::RefCounted<UnzipParams>,
                    public unzip::mojom::UnzipFilter {
 public:
  UnzipParams(mojo::PendingRemote<mojom::Unzipper> unzipper,
              UnzipCallback callback)
      : unzipper_(std::move(unzipper)), callback_(std::move(callback)) {}

  mojo::Remote<mojom::Unzipper>& unzipper() { return unzipper_; }

  void InvokeCallback(bool result) {
    if (callback_)
      std::move(callback_).Run(result);

    unzipper_.reset();
  }

  void SetFilter(
      mojo::PendingReceiver<unzip::mojom::UnzipFilter> filter_receiver,
      UnzipFilterCallback filter_callback) {
    DCHECK(filter_callback);
    filter_receiver_.Bind(std::move(filter_receiver));
    filter_callback_ = std::move(filter_callback);
  }

 private:
  friend class base::RefCounted<UnzipParams>;

  ~UnzipParams() override = default;

  // unzip::mojom::UnzipFilter implementation:
  void ShouldUnzipFile(const base::FilePath& path,
                       ShouldUnzipFileCallback callback) override {
    DCHECK(filter_callback_);
    std::move(callback).Run(filter_callback_.Run(path));
  }

  // The Remote and UnzipFilter are stored so they do not get deleted before the
  // callback runs.
  mojo::Remote<mojom::Unzipper> unzipper_;
  mojo::Receiver<unzip::mojom::UnzipFilter> filter_receiver_{this};
  UnzipFilterCallback filter_callback_;
  UnzipCallback callback_;
};

class DetectEncodingParams : public base::RefCounted<DetectEncodingParams> {
 public:
  DetectEncodingParams(mojo::PendingRemote<mojom::Unzipper> unzipper,
                       DetectEncodingCallback callback)
      : unzipper_(std::move(unzipper)), callback_(std::move(callback)) {}

  mojo::Remote<mojom::Unzipper>& unzipper() { return unzipper_; }

  void InvokeCallback(int encoding) {
    if (callback_) {
      base::UmaHistogramEnumeration("Unzipper.DetectEncoding.Result",
                                    static_cast<Encoding>(encoding),
                                    Encoding::NUM_ENCODINGS);
      base::UmaHistogramTimes("Unzipper.DetectEncoding.Time",
                              base::TimeTicks::Now() - start_time_);

      std::move(callback_).Run(static_cast<Encoding>(encoding));
    }

    unzipper_.reset();
  }

 private:
  friend class base::RefCounted<DetectEncodingParams>;

  ~DetectEncodingParams() = default;

  // The Remote is stored so it does not get deleted before the callback runs.
  mojo::Remote<mojom::Unzipper> unzipper_;
  DetectEncodingCallback callback_;
  const base::TimeTicks start_time_ = base::TimeTicks::Now();
};

void DoUnzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
             const base::FilePath& zip_path,
             const base::FilePath& output_dir,
             UnzipFilterCallback filter_callback,
             mojom::UnzipOptionsPtr options,
             UnzipCallback result_callback) {
  base::File zip_file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!zip_file.IsValid()) {
    LOG(ERROR) << "Cannot open ZIP archive " << Redact(zip_path) << ": "
               << base::File::ErrorToString(zip_file.error_details());
    std::move(result_callback).Run(false);
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
      std::move(unzipper), std::move(result_callback));

  unzip_params->unzipper().set_disconnect_handler(
      base::BindOnce(&UnzipParams::InvokeCallback, unzip_params, false));

  mojo::PendingRemote<unzip::mojom::UnzipFilter> filter_remote;
  if (filter_callback) {
    unzip_params->SetFilter(filter_remote.InitWithNewPipeAndPassReceiver(),
                            std::move(filter_callback));
  }

  unzip_params->unzipper()->Unzip(
      std::move(zip_file), std::move(directory_remote), std::move(options),
      std::move(filter_remote),
      base::BindOnce(&UnzipParams::InvokeCallback, unzip_params));
}

void DoDetectEncoding(mojo::PendingRemote<mojom::Unzipper> unzipper,
                      const base::FilePath& zip_path,
                      DetectEncodingCallback result_callback) {
  base::File zip_file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!zip_file.IsValid()) {
    LOG(ERROR) << "Cannot open ZIP archive " << Redact(zip_path) << ": "
               << base::File::ErrorToString(zip_file.error_details());
    std::move(result_callback).Run(UNKNOWN_ENCODING);
    return;
  }

  // |result_callback| is shared between the connection error handler and the
  // DetectEncoding call using a refcounted DetectEncodingParams object that
  // owns |result_callback|.
  auto params = base::MakeRefCounted<DetectEncodingParams>(
      std::move(unzipper), std::move(result_callback));

  params->unzipper().set_disconnect_handler(base::BindOnce(
      &DetectEncodingParams::InvokeCallback, params, UNKNOWN_ENCODING));

  params->unzipper()->DetectEncoding(
      std::move(zip_file),
      base::BindOnce(&DetectEncodingParams::InvokeCallback, params));
}

}  // namespace

void Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
           const base::FilePath& zip_path,
           const base::FilePath& output_dir,
           UnzipCallback callback) {
  UnzipWithFilter(std::move(unzipper), zip_path, output_dir,
                  UnzipFilterCallback(), std::move(callback));
}

void Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
           const base::FilePath& zip_file,
           const base::FilePath& output_dir,
           mojom::UnzipOptionsPtr options,
           UnzipCallback result_callback) {
  DCHECK(!result_callback.is_null());

  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DoUnzip, std::move(unzipper), zip_file, output_dir,
                     UnzipFilterCallback(), std::move(options),
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(result_callback))));
}

void UnzipWithFilter(mojo::PendingRemote<mojom::Unzipper> unzipper,
                     const base::FilePath& zip_path,
                     const base::FilePath& output_dir,
                     UnzipFilterCallback filter_callback,
                     UnzipCallback result_callback) {
  DCHECK(!result_callback.is_null());

  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DoUnzip, std::move(unzipper), zip_path, output_dir,
                     filter_callback, unzip::mojom::UnzipOptions::New(),
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(result_callback))));
}

void DetectEncoding(mojo::PendingRemote<mojom::Unzipper> unzipper,
                    const base::FilePath& zip_path,
                    DetectEncodingCallback result_callback) {
  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DoDetectEncoding, std::move(unzipper), zip_path,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(result_callback))));
}

}  // namespace unzip
