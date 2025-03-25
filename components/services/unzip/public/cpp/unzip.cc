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
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace unzip {

namespace {

std::string Redact(const base::FilePath& path) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", path.AsUTF8Unsafe(), "'"})
                         : "(redacted)";
}

bool CheckZipFileValid(const base::FilePath& zip_path,
                       const base::File& zip_file) {
  if (!zip_file.IsValid()) {
    LOG(ERROR) << "Cannot open ZIP archive " << Redact(zip_path) << ": "
               << base::File::ErrorToString(zip_file.error_details());
    return false;
  }
  return true;
}

// An UnzipOperation is a cancellable invocation of Unzip. To support
// cancellation, it maintains the completion callback as a class member.
// UnzipOperations represent only a single operation and must not be reused.
class UnzipOperation : public base::RefCountedThreadSafe<UnzipOperation>,
                       public unzip::mojom::UnzipFilter,
                       public unzip::mojom::UnzipListener {
 public:
  UnzipOperation(UnzipFilterCallback filter_callback,
                 UnzipListenerCallback listener_callback,
                 UnzipCallback callback)
      : filter_callback_(filter_callback),
        listener_callback_(listener_callback),
        callback_(std::move(callback)) {
    // UnzipOperation is created on one sequence but forever used on another.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // Start must be called only once. May block.
  void Start(mojo::PendingRemote<mojom::Unzipper> unzipper,
             const base::FilePath& zip_path,
             const base::FilePath& output_dir,
             mojom::UnzipOptionsPtr options) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    unzipper_.Bind(std::move(unzipper));

    // Open zip_path.
    base::File zip_file(zip_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!CheckZipFileValid(zip_path, zip_file)) {
      Done(false);
      return;
    }

    // Set up unzipper remotes/receivers.
    unzipper_.set_disconnect_handler(
        base::BindOnce(&UnzipOperation::Done, this, false));
    mojo::PendingRemote<storage::mojom::Directory> directory_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<storage::FilesystemImpl>(
            // kTrusted is set to allow the unzipping process to handle all
            // types of files.
            output_dir, storage::FilesystemImpl::ClientType::kTrusted),
        directory_remote.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<unzip::mojom::UnzipFilter> filter_remote;
    filter_receiver_.Bind(filter_remote.InitWithNewPipeAndPassReceiver());

    // Start unzipping.
    unzipper_->Unzip(std::move(zip_file), std::move(directory_remote),
                     std::move(options), std::move(filter_remote),
                     listener_receiver_.BindNewPipeAndPassRemote(),
                     base::BindOnce(&UnzipOperation::Done, this));
  }

  // Resets the unzipper remote and triggers the completion callback.
  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Done(false);
  }

 private:
  // Done may be called multiple times, for example if cancellation is posted
  // to a task runner, but concurrently, the job completes or the remote
  // disconnects.
  void Done(bool result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback_).Run(result);
    callback_ = base::DoNothing();
    unzipper_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<UnzipOperation>;

  ~UnzipOperation() override = default;

  // unzip::mojom::UnzipFilter implementation:
  void ShouldUnzipFile(const base::FilePath& path,
                       ShouldUnzipFileCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(filter_callback_.Run(path));
  }

  // unzip::mojom::UnzipListener implementation:
  void OnProgress(uint64_t bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    listener_callback_.Run(bytes);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<mojom::Unzipper> unzipper_;
  mojo::Receiver<unzip::mojom::UnzipFilter> filter_receiver_{this};
  UnzipFilterCallback filter_callback_;
  mojo::Receiver<unzip::mojom::UnzipListener> listener_receiver_{this};
  UnzipListenerCallback listener_callback_;
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

base::OnceClosure Unzip(mojo::PendingRemote<mojom::Unzipper> unzipper,
                        const base::FilePath& zip_file,
                        const base::FilePath& output_dir,
                        mojom::UnzipOptionsPtr options,
                        UnzipFilterCallback filter_callback,
                        UnzipListenerCallback listener_callback,
                        UnzipCallback result_callback) {
  CHECK(result_callback);

  auto unzip_operation = base::MakeRefCounted<UnzipOperation>(
      filter_callback,
      base::BindPostTaskToCurrentDefault(std::move(listener_callback)),
      base::BindPostTaskToCurrentDefault(std::move(result_callback)));

  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  runner->PostTask(FROM_HERE,
                   base::BindOnce(&UnzipOperation::Start, unzip_operation,
                                  std::move(unzipper), zip_file, output_dir,
                                  std::move(options)));

  return base::BindPostTask(
      runner, base::BindOnce(&UnzipOperation::Cancel, unzip_operation));
}

void DetectEncoding(mojo::PendingRemote<mojom::Unzipper> unzipper,
                    const base::FilePath& zip_path,
                    DetectEncodingCallback result_callback) {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce(&DoDetectEncoding,
                                           std::move(unzipper), zip_path,
                                           base::BindPostTaskToCurrentDefault(
                                               std::move(result_callback))));
}

void GetExtractedInfo(mojo::PendingRemote<mojom::Unzipper> unzipper,
                      const base::FilePath& zip_path,
                      GetExtractedInfoCallback result_callback) {
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce(&DoGetExtractedInfo,
                                           std::move(unzipper), zip_path,
                                           base::BindPostTaskToCurrentDefault(
                                               std::move(result_callback))));
}

UnzipFilterCallback AllContents() {
  return base::BindRepeating([](const base::FilePath&) { return true; });
}

}  // namespace unzip
