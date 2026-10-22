// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/public/cpp/unzip.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"
#include "components/services/storage/public/mojom/filesystem/directory.mojom.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

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

// Normalizes and validates a relative path supplied by the unzipper utility
// process before passing it to `filter_callback_` or the underlying filesystem:
// - rejects empty, absolute, parent-referencing (`..`), and current-directory
//   (`.`) paths;
// - rejects paths with a trailing separator unless `allow_trailing_separator`
//   is set, and otherwise strips the trailing separator;
// - on Windows, trims trailing dots and spaces from each component and rejects
//   any component containing `:` (e.g. drive-relative paths or NTFS alternate
//   data streams) or path separators.
std::optional<base::FilePath> NormalizeUnzipRelativePath(
    const base::FilePath& path,
    bool allow_trailing_separator = false) {
  if (path.empty() || path.IsAbsolute() || path.ReferencesParent()) {
    return std::nullopt;
  }
  if (!allow_trailing_separator && path.EndsWithSeparator()) {
    return std::nullopt;
  }
  std::vector<base::FilePath::StringType> components = path.GetComponents();
  if (components.empty()) {
    return std::nullopt;
  }
  base::FilePath normalized;
  for (const auto& comp : components) {
#if BUILDFLAG(IS_WIN)
    // On Windows, Win32 path normalization strips trailing dots and spaces from
    // the final path component when creating, opening, or deleting files and
    // directories (so "foo.", "foo ", and "foo" all refer to "foo" on disk).
    // Canonicalize each component on Windows so `filter_callback_` and
    // `created_files_` see the exact path that the OS will access, and so
    // intermediate directory components match the directories created by
    // `CreateDirectory`. Also reject components containing `:` (such as
    // drive-relative paths or NTFS alternate data streams like `::$DATA`) or
    // path separators (such as root-relative `\foo`).
    base::FilePath::StringViewType canonical =
        base::TrimString(comp, FILE_PATH_LITERAL(". "), base::TRIM_TRAILING);
    if (canonical.find_first_of(FILE_PATH_LITERAL(":\\/")) !=
        base::FilePath::StringViewType::npos) {
      return std::nullopt;
    }
#else
    base::FilePath::StringViewType canonical = comp;
#endif
    if (canonical.empty() || canonical == base::FilePath::kCurrentDirectory ||
        canonical == base::FilePath::kParentDirectory) {
      return std::nullopt;
    }
    normalized = normalized.empty() ? base::FilePath(canonical)
                                    : normalized.Append(canonical);
  }
  return normalized;
}

// A `storage::mojom::Directory` implementation that restricts file operations
// to those required for unzipping (`OpenFile`, `CreateDirectory`, and
// `DeleteFile`) by delegating only those operations to a private
// `storage::FilesystemImpl`, and binds a single Directory receiver owned by
// `UnzipOperation` so that directory access is revoked as soon as the unzip
// operation finishes.
class UnzipFilesystemImpl : public storage::mojom::Directory {
 public:
  // `ClientType::kTrusted` is set to allow the unzipping process to handle all
  // types of files.
  UnzipFilesystemImpl(const base::FilePath& root,
                      mojo::PendingReceiver<storage::mojom::Directory> receiver,
                      UnzipFilterCallback filter_callback)
      : filesystem_(root, storage::FilesystemImpl::ClientType::kTrusted),
        filter_callback_(std::move(filter_callback)),
        receiver_(this, std::move(receiver)) {}

  UnzipFilesystemImpl(const UnzipFilesystemImpl&) = delete;
  UnzipFilesystemImpl& operator=(const UnzipFilesystemImpl&) = delete;
  ~UnzipFilesystemImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // storage::mojom::Directory:
  void Clone(
      mojo::PendingReceiver<storage::mojom::Directory> receiver) override {
    receiver_.ReportBadMessage("Unexpected Directory::Clone from unzipper");
  }

  void PathExists(const base::FilePath& path,
                  PathExistsCallback callback) override {
    receiver_.ReportBadMessage(
        "Unexpected Directory::PathExists from unzipper");
  }

  void GetEntries(const base::FilePath& path,
                  storage::mojom::GetEntriesMode mode,
                  GetEntriesCallback callback) override {
    receiver_.ReportBadMessage(
        "Unexpected Directory::GetEntries from unzipper");
  }

  void OpenFile(const base::FilePath& path,
                storage::mojom::FileOpenMode mode,
                storage::mojom::FileReadAccess read_access,
                storage::mojom::FileWriteAccess write_access,
                OpenFileCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::optional<base::FilePath> normalized = NormalizeUnzipRelativePath(path);
    if (!normalized ||
        mode != storage::mojom::FileOpenMode::kCreateAndOpenOnlyIfNotExists ||
        read_access != storage::mojom::FileReadAccess::kReadAllowed ||
        write_access != storage::mojom::FileWriteAccess::kWriteAllowed ||
        created_files_.contains(*normalized) ||
        !filter_callback_.Run(*normalized)) {
      std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED,
                              base::File());
      return;
    }
    filesystem_.OpenFile(
        *normalized, mode, read_access, write_access,
        base::BindOnce(
            [](UnzipFilesystemImpl* self, base::FilePath created_path,
               OpenFileCallback cb, base::File::Error error, base::File file) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
              if (error == base::File::FILE_OK) {
                self->created_files_.insert(std::move(created_path));
              }
              std::move(cb).Run(error, std::move(file));
            },
            base::Unretained(this), *normalized, std::move(callback)));
  }

  void CreateDirectory(const base::FilePath& path,
                       CreateDirectoryCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // `CreateDirectory` is intentionally not passed through `filter_callback_`
    // because a filter may reject directory entries (e.g. theme filters that
    // only match file extensions) while still allowing nested files inside
    // them.
    std::optional<base::FilePath> normalized =
        NormalizeUnzipRelativePath(path, /*allow_trailing_separator=*/true);
    if (!normalized) {
      std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
      return;
    }
    filesystem_.CreateDirectory(*normalized, std::move(callback));
  }

  void DeleteFile(const base::FilePath& path,
                  DeleteFileCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::optional<base::FilePath> normalized = NormalizeUnzipRelativePath(path);
    if (!normalized || !created_files_.contains(*normalized)) {
      std::move(callback).Run(false);
      return;
    }
    filesystem_.DeleteFile(
        *normalized,
        base::BindOnce(
            [](UnzipFilesystemImpl* self, base::FilePath deleted_path,
               DeleteFileCallback cb, bool success) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
              if (success) {
                self->created_files_.erase(deleted_path);
              }
              std::move(cb).Run(success);
            },
            base::Unretained(this), *normalized, std::move(callback)));
  }

  void GetFileInfo(const base::FilePath& path,
                   GetFileInfoCallback callback) override {
    receiver_.ReportBadMessage(
        "Unexpected Directory::GetFileInfo from unzipper");
  }

  void GetDiskSpaceInfo(const base::FilePath& path,
                        GetDiskSpaceInfoCallback callback) override {
    receiver_.ReportBadMessage(
        "Unexpected Directory::GetDiskSpaceInfo from unzipper");
  }

  void ComputeDirectorySize(const base::FilePath& path,
                            ComputeDirectorySizeCallback callback) override {
    receiver_.ReportBadMessage(
        "Unexpected Directory::ComputeDirectorySize from unzipper");
  }

  void GetPathAccess(const base::FilePath& path,
                     GetPathAccessCallback callback) override {
    receiver_.ReportBadMessage(
        "Unexpected Directory::GetPathAccess from unzipper");
  }

  void RenameFile(const base::FilePath& old_path,
                  const base::FilePath& new_path,
                  RenameFileCallback callback) override {
    receiver_.ReportBadMessage(
        "Unexpected Directory::RenameFile from unzipper");
  }

  void LockFile(const base::FilePath& path,
                LockFileCallback callback) override {
    receiver_.ReportBadMessage("Unexpected Directory::LockFile from unzipper");
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  storage::FilesystemImpl filesystem_;
  UnzipFilterCallback filter_callback_;
  absl::flat_hash_set<base::FilePath> created_files_;
  mojo::Receiver<storage::mojom::Directory> receiver_;
};

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
    directory_ = std::make_unique<UnzipFilesystemImpl>(
        output_dir, directory_remote.InitWithNewPipeAndPassReceiver(),
        filter_callback_);
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
  friend class base::RefCountedThreadSafe<UnzipOperation>;

  // Done may be called multiple times, for example if cancellation is posted
  // to a task runner, but concurrently, the job completes or the remote
  // disconnects.
  void Done(bool result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Revoke all capabilities granted to the utility process before signalling
    // completion so no file operation can occur after the caller observes
    // completion.
    directory_.reset();
    filter_receiver_.reset();
    listener_receiver_.reset();
    unzipper_.reset();
    std::exchange(callback_, base::DoNothing()).Run(result);
  }

  ~UnzipOperation() override = default;

  // unzip::mojom::UnzipFilter implementation:
  void ShouldUnzipFile(const base::FilePath& path,
                       ShouldUnzipFileCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::optional<base::FilePath> normalized =
        NormalizeUnzipRelativePath(path, /*allow_trailing_separator=*/true);
    if (!normalized) {
      std::move(callback).Run(false);
      return;
    }
    std::move(callback).Run(filter_callback_.Run(*normalized));
  }

  // unzip::mojom::UnzipListener implementation:
  void OnProgress(uint64_t bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    listener_callback_.Run(bytes);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<mojom::Unzipper> unzipper_;
  std::unique_ptr<UnzipFilesystemImpl> directory_;
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
  void Start(mojo::PendingRemote<mojom::Unzipper> unzipper,
             const base::FilePath& in_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::File in(in_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                               base::File::FLAG_WIN_SEQUENTIAL_SCAN |
                               base::File::FLAG_WIN_SHARE_DELETE);
    if (!in.IsValid()) {
      Done(false);
      return;
    }
    base::File out(out_path_, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                  base::File::FLAG_WRITE |
                                  base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                  base::File::FLAG_WIN_SEQUENTIAL_SCAN |
                                  base::File::FLAG_WIN_SHARE_DELETE);
    if (!out.IsValid()) {
      Done(false);
      return;
    }
    unzipper_.Bind(std::move(unzipper));
    unzipper_.set_disconnect_handler(
        base::BindOnce(&DecodeOperation::Done, this, false));
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
      base::GetDeleteFileCallback(out_path_).Run();
    }
    std::move(callback_).Run(result);
    callback_ = base::DoNothing();
    unzipper_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<DecodeOperation>;

  ~DecodeOperation() = default;

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<mojom::Unzipper> unzipper_;
  const base::FilePath out_path_;
  base::OnceCallback<void(bool)> callback_;
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
  CHECK(filter_callback);

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

base::OnceClosure DecodeXz(mojo::PendingRemote<mojom::Unzipper> unzipper,
                           const base::FilePath& in_file,
                           const base::FilePath& out_file,
                           base::OnceCallback<void(bool)> callback) {
  auto op =
      base::MakeRefCounted<DecodeOperation>(out_file, std::move(callback));
  const scoped_refptr<base::SequencedTaskRunner> runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  runner->PostTask(FROM_HERE, base::BindOnce(&DecodeOperation::Start, op,
                                             std::move(unzipper), in_file));
  return base::BindPostTask(runner,
                            base::BindOnce(&DecodeOperation::Cancel, op));
}

}  // namespace unzip
