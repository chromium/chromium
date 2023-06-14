// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace unzip {

class UnzipParams : public base::RefCounted<UnzipParams>,
                    public unzip::mojom::UnzipFilter,
                    public unzip::mojom::UnzipListener {
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

  void SetListener(UnzipListenerCallback listener_callback) {
    DCHECK(listener_callback);
    listener_callback_ = std::move(listener_callback);
  }

  mojo::Receiver<unzip::mojom::UnzipListener>* GetListenerReceiver() {
    return &listener_receiver_;
  }

  // Set by the task runner when it resets the UnzipParams object.
  base::AtomicFlag clean_up_is_done;

 private:
  friend class base::RefCounted<UnzipParams>;

  ~UnzipParams() override = default;

  // unzip::mojom::UnzipFilter implementation:
  void ShouldUnzipFile(const base::FilePath& path,
                       ShouldUnzipFileCallback callback) override {
    DCHECK(filter_callback_);
    std::move(callback).Run(filter_callback_.Run(path));
  }

  // unzip::mojom::UnzipListener implementation:
  void OnProgress(uint64_t bytes) override {
    DCHECK(listener_callback_);
    listener_callback_.Run(bytes);
  }

  // The Remote, UnzipFilter and UnzipListener are stored so they do not
  // get deleted before the callback runs.
  mojo::Remote<mojom::Unzipper> unzipper_;
  mojo::Receiver<unzip::mojom::UnzipFilter> filter_receiver_{this};
  UnzipFilterCallback filter_callback_;
  mojo::Receiver<unzip::mojom::UnzipListener> listener_receiver_{this};
  UnzipListenerCallback listener_callback_;
  UnzipCallback callback_;
};

namespace {

std::string Redact(const base::FilePath& path) {
  return LOG_IS_ON(INFO) ? "'" + path.AsUTF8Unsafe() + "'" : "(redacted)";
}

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

class GetExtractedInfoParams : public base::RefCounted<GetExtractedInfoParams> {
 public:
  GetExtractedInfoParams(mojo::PendingRemote<mojom::Unzipper> unzipper,
                         GetExtractedInfoCallback callback)
      : unzipper_(std::move(unzipper)), callback_(std::move(callback)) {}

  mojo::Remote<mojom::Unzipper>& unzipper() { return unzipper_; }

  void InvokeCallback(mojom::InfoPtr info) {
    if (callback_) {
      // TODO(crbug.com/953256) Add UMA timing.
      std::move(callback_).Run(std::move(info));
    }

    unzipper_.reset();
  }

 private:
  friend class base::RefCounted<GetExtractedInfoParams>;

  ~GetExtractedInfoParams() = default;

  // The Remote is stored so it does not get deleted before the callback runs.
  mojo::Remote<mojom::Unzipper> unzipper_;
  GetExtractedInfoCallback callback_;
};

bool CheckZipFileValid(const base::FilePath& zip_path,
                       const base::File& zip_file) {
  if (!zip_file.IsValid()) {
    LOG(ERROR) << "Cannot open ZIP archive " << Redact(zip_path) << ": "
               << base::File::ErrorToString(zip_file.error_details());
    return false;
  }
  return true;
}

void PrepareUnzipParams(
    scoped_refptr<UnzipParams> unzip_params,
    const base::FilePath& output_dir,
    UnzipFilterCallback filter_callback,
    UnzipListenerCallback listener_callback,
    mojo::PendingRemote<storage::mojom::Directory>& directory_remote,
    mojo::PendingRemote<unzip::mojom::UnzipFilter>& filter_remote,
    mojo::PendingRemote<unzip::mojom::UnzipListener>& listener_remote) {
  // kTrusted is set to allow the unzipping process to handle all types of
  // files.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<storage::FilesystemImpl>(
          output_dir, storage::FilesystemImpl::ClientType::kTrusted),
      directory_remote.InitWithNewPipeAndPassReceiver());

  unzip_params->unzipper().set_disconnect_handler(
      base::BindOnce(&UnzipParams::InvokeCallback, unzip_params, false));

  if (filter_callback) {
    unzip_params->SetFilter(filter_remote.InitWithNewPipeAndPassReceiver(),
                            std::move(filter_callback));
  }

  if (listener_callback) {
    mojo::Receiver<unzip::mojom::UnzipListener>* listener =
        unzip_params->GetListenerReceiver();
    unzip_params->SetListener(std::move(listener_callback));
    listener_remote = listener->BindNewPipeAndPassRemote();
  }
}

void DoUnzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
             const base::FilePath& zip_path,
             const base::FilePath& output_dir,
             UnzipFilterCallback filter_callback,
             UnzipListenerCallback listener_callback,
             mojom::UnzipOptionsPtr options,
             UnzipCallback result_callback) {
  base::File zip_file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!CheckZipFileValid(zip_path, zip_file)) {
    std::move(result_callback).Run(false);
    return;
  }

  // |result_callback| is shared between the connection error handler and the
  // Unzip call using a refcounted UnzipParams object that owns
  // |result_callback|.
  auto unzip_params = base::MakeRefCounted<UnzipParams>(
      std::move(unzipper), std::move(result_callback));

  mojo::PendingRemote<storage::mojom::Directory> directory_remote;
  mojo::PendingRemote<unzip::mojom::UnzipFilter> filter_remote;
  mojo::PendingRemote<unzip::mojom::UnzipListener> listener_remote;
  PrepareUnzipParams(unzip_params, output_dir, filter_callback,
                     listener_callback, directory_remote, filter_remote,
                     listener_remote);

  unzip_params->unzipper()->Unzip(
      std::move(zip_file), std::move(directory_remote), std::move(options),
      std::move(filter_remote), std::move(listener_remote),
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

void DoGetExtractedInfo(mojo::PendingRemote<mojom::Unzipper> unzipper,
                        const base::FilePath& zip_path,
                        GetExtractedInfoCallback result_callback) {
  base::File zip_file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  unzip::mojom::InfoPtr info = unzip::mojom::Info::New(false, 0, false, false);
  if (!zip_file.IsValid()) {
    LOG(ERROR) << "Cannot open ZIP archive " << Redact(zip_path) << ": "
               << base::File::ErrorToString(zip_file.error_details());
    std::move(result_callback).Run(std::move(info));
    return;
  }

  // |result_callback| is shared between the connection error handler and the
  // GetExtractedInfo call using a refcounted GetExtractedInfoParams object that
  // owns |result_callback|.
  auto params = base::MakeRefCounted<GetExtractedInfoParams>(
      std::move(unzipper), std::move(result_callback));

  params->unzipper().set_disconnect_handler(base::BindOnce(
      &GetExtractedInfoParams::InvokeCallback, params, std::move(info)));

  params->unzipper()->GetExtractedInfo(
      std::move(zip_file),
      base::BindOnce(&GetExtractedInfoParams::InvokeCallback, params));
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
           UnzipListenerCallback listener_callback,
           UnzipCallback result_callback) {
  DCHECK(!result_callback.is_null());

  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DoUnzip, std::move(unzipper), zip_file, output_dir,
          UnzipFilterCallback(),
          base::BindPostTaskToCurrentDefault(std::move(listener_callback)),
          std::move(options),
          base::BindPostTaskToCurrentDefault(std::move(result_callback))));
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
      base::BindOnce(
          &DoUnzip, std::move(unzipper), zip_path, output_dir, filter_callback,
          UnzipListenerCallback(), unzip::mojom::UnzipOptions::New(),
          base::BindPostTaskToCurrentDefault(std::move(result_callback))));
}

void DetectEncoding(mojo::PendingRemote<mojom::Unzipper> unzipper,
                    const base::FilePath& zip_path,
                    DetectEncodingCallback result_callback) {
  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(FROM_HERE, base::BindOnce(&DoDetectEncoding,
                                             std::move(unzipper), zip_path,
                                             base::BindPostTaskToCurrentDefault(
                                                 std::move(result_callback))));
}

void GetExtractedInfo(mojo::PendingRemote<mojom::Unzipper> unzipper,
                      const base::FilePath& zip_path,
                      GetExtractedInfoCallback result_callback) {
  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(FROM_HERE, base::BindOnce(&DoGetExtractedInfo,
                                             std::move(unzipper), zip_path,
                                             base::BindPostTaskToCurrentDefault(
                                                 std::move(result_callback))));
}

ZipFileUnpacker::ZipFileUnpacker() = default;

ZipFileUnpacker::~ZipFileUnpacker() = default;

void DoUnzipWithParams(mojo::PendingRemote<mojom::Unzipper> unzipper,
                       scoped_refptr<UnzipParams>& unzip_params,
                       const base::FilePath& zip_path,
                       const base::FilePath& output_dir,
                       UnzipFilterCallback filter_callback,
                       UnzipListenerCallback listener_callback,
                       mojom::UnzipOptionsPtr options,
                       UnzipCallback result_callback,
                       file_access::ScopedFileAccess file_access) {
  base::File zip_file(zip_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!CheckZipFileValid(zip_path, zip_file)) {
    std::move(result_callback).Run(false);
    return;
  }

  // |result_callback| is shared between the connection error handler and the
  // Unzip call using a refcounted UnzipParams object that owns
  // |result_callback|.
  unzip_params = base::MakeRefCounted<UnzipParams>(std::move(unzipper),
                                                   std::move(result_callback));

  mojo::PendingRemote<storage::mojom::Directory> directory_remote;
  mojo::PendingRemote<unzip::mojom::UnzipFilter> filter_remote;
  mojo::PendingRemote<unzip::mojom::UnzipListener> listener_remote;
  PrepareUnzipParams(unzip_params, output_dir, filter_callback,
                     listener_callback, directory_remote, filter_remote,
                     listener_remote);

  unzip_params->unzipper()->Unzip(
      std::move(zip_file), std::move(directory_remote), std::move(options),
      std::move(filter_remote), std::move(listener_remote),
      base::BindOnce(&UnzipParams::InvokeCallback, unzip_params));
}

void ZipFileUnpacker::Unpack(mojo::PendingRemote<mojom::Unzipper> unzipper,
                             const base::FilePath& zip_file,
                             const base::FilePath& output_dir,
                             mojom::UnzipOptionsPtr options,
                             UnzipListenerCallback listener_callback,
                             UnzipCallback result_callback) {
  DCHECK(!result_callback.is_null());

  file_access::RequestFilesAccessForSystem(
      {zip_file},
      base::BindPostTask(
          runner_,
          base::BindOnce(
              &::unzip::DoUnzipWithParams, std::move(unzipper),
              std::ref(params_), zip_file, output_dir, UnzipFilterCallback(),
              base::BindPostTaskToCurrentDefault(std::move(listener_callback)),
              std::move(options),
              base::BindPostTaskToCurrentDefault(std::move(result_callback)))));
}

void ReleaseParams(scoped_refptr<UnzipParams>& unzip_params) {
  if (unzip_params) {
    unzip_params->clean_up_is_done.Set();
    unzip_params.reset();
  }
}

void EndUnpack(scoped_refptr<UnzipParams>& unzip_params) {
  if (unzip_params) {
    unzip_params->InvokeCallback(false);
    ReleaseParams(unzip_params);
  }
}

void ZipFileUnpacker::Stop() {
  runner_->PostTask(FROM_HERE, base::BindOnce(&EndUnpack, std::ref(params_)));
}

bool ZipFileUnpacker::CleanUpDone() {
  if (params_) {
    return params_->clean_up_is_done.IsSet();
  }
  return true;
}

void ZipFileUnpacker::CleanUp() {
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(&ReleaseParams, std::ref(params_)));
}

}  // namespace unzip
