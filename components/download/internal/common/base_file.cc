// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/base_file.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/quarantine/quarantine.h"
#include "crypto/secure_hash.h"

#if defined(OS_WIN)
#include "components/services/quarantine/public/cpp/quarantine_features_win.h"
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "components/download/internal/common/android/download_collection_bridge.h"
#endif  // defined(OS_ANDROID)

#define CONDITIONAL_TRACE(trace)                  \
  do {                                            \
    if (download_id_ != DownloadItem::kInvalidId) \
      TRACE_EVENT_##trace;                        \
  } while (0)

namespace download {

namespace {
class FileErrorData : public base::trace_event::ConvertableToTraceFormat {
 public:
  FileErrorData(const char* operation,
                int os_error,
                DownloadInterruptReason interrupt_reason)
      : operation_(operation),
        os_error_(os_error),
        interrupt_reason_(interrupt_reason) {}

  ~FileErrorData() override = default;

  void AppendAsTraceFormat(std::string* out) const override {
    out->append("{");
    out->append(
        base::StringPrintf("\"operation\":\"%s\",", operation_.c_str()));
    out->append(base::StringPrintf("\"os_error\":\"%d\",", os_error_));
    out->append(base::StringPrintf(
        "\"interrupt_reason\":\"%s\",",
        DownloadInterruptReasonToString(interrupt_reason_).c_str()));
    out->append("}");
  }

 private:
  std::string operation_;
  int os_error_;
  DownloadInterruptReason interrupt_reason_;
  DISALLOW_COPY_AND_ASSIGN(FileErrorData);
};

void InitializeFile(base::File* file, const base::FilePath& file_path) {
#if defined(OS_ANDROID)
  if (file_path.IsContentUri()) {
    *file = DownloadCollectionBridge::OpenIntermediateUri(file_path);
    return;
  }
#endif  // defined(OS_ANDROID)

  // Use exclusive write to prevent another process from writing the file.
  file->Initialize(
      file_path,
      base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
          base::File::FLAG_READ
#if defined(OS_WIN)
          // Don't allow other process to write to the file while Chrome is
          // writing to it. On posix systems, use FLAG_EXCLUSIVE_WRITE will
          // cause file creation to fail if the file already exists.
          | base::File::FLAG_EXCLUSIVE_WRITE
#endif  // defined(OS_WIN)
  );
}

void DeleteFile(const base::FilePath& file_path) {
#if defined(OS_ANDROID)
  if (file_path.IsContentUri()) {
    DownloadCollectionBridge::DeleteIntermediateUri(file_path);
    return;
  }
#endif  // defined(OS_ANDROID)
  base::DeleteFile(file_path, false);
}

}  // namespace

BaseFile::BaseFile(uint32_t download_id) : download_id_(download_id) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

BaseFile::~BaseFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (detached_)
    Close();
  else
    Cancel();  // Will delete the file.
}

DownloadInterruptReason BaseFile::Initialize(
    const base::FilePath& full_path,
    const base::FilePath& default_directory,
    base::File file,
    int64_t bytes_so_far,
    const std::string& hash_so_far,
    std::unique_ptr<crypto::SecureHash> hash_state,
    bool is_sparse_file,
    int64_t* const bytes_wasted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!detached_);

  if (full_path.empty()) {
    base::FilePath temp_file;
    if ((default_directory.empty() ||
         !base::CreateTemporaryFileInDir(default_directory, &temp_file)) &&
        !base::CreateTemporaryFile(&temp_file)) {
      return LogInterruptReason("Unable to create", 0,
                                DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
    }
    full_path_ = temp_file;
  } else {
    full_path_ = full_path;
  }

  bytes_so_far_ = bytes_so_far;
  secure_hash_ = std::move(hash_state);
  is_sparse_file_ = is_sparse_file;
  // Sparse file doesn't validate hash.
  if (is_sparse_file_)
    secure_hash_.reset();
  file_ = std::move(file);

  return Open(hash_so_far, bytes_wasted);
}

DownloadInterruptReason BaseFile::AppendDataToFile(const char* data,
                                                   size_t data_len) {
  DCHECK(!is_sparse_file_);
  return WriteDataToFile(bytes_so_far_, data, data_len);
}

DownloadInterruptReason BaseFile::WriteDataToFile(int64_t offset,
                                                  const char* data,
                                                  size_t data_len) {
  // NOTE(benwells): The above DCHECK won't be present in release builds,
  // so we log any occurences to see how common this error is in the wild.
  if (detached_)
    RecordDownloadCount(APPEND_TO_DETACHED_FILE_COUNT);

  if (!file_.IsValid()) {
    return LogInterruptReason("No file stream on append", 0,
                              DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  }

  // TODO(phajdan.jr): get rid of this check.
  if (data_len == 0)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  // Use nestable async event instead of sync event so that all the writes
  // belong to the same download will be grouped together.
  CONDITIONAL_TRACE(
      NESTABLE_ASYNC_BEGIN0("download", "DownloadFileWrite", download_id_));

  if (bytes_so_far_ != offset) {
    // A hole is created in the file.
    is_sparse_file_ = true;
    secure_hash_.reset();
  }

  // Writes to the file.
  int64_t len = base::saturated_cast<int64_t>(data_len);
  const char* current_data = data;
  int64_t current_offset = offset;
  while (len > 0) {
    // |write_result| may be less than |len|, and return an error on the next
    // write call when the disk is unavaliable.
    int write_result = file_.Write(current_offset, current_data, len);
    DCHECK_NE(0, write_result);

    // Report errors on file writes.
    if (write_result < 0)
      return LogSystemError("Write", logging::GetLastSystemErrorCode());

    // Update status.
    DCHECK_LE(write_result, len);
    len -= write_result;
    current_data += write_result;
    current_offset += write_result;
    bytes_so_far_ += write_result;
  }

  CONDITIONAL_TRACE(NESTABLE_ASYNC_END1("download", "DownloadFileWrite",
                                        download_id_, "bytes", data_len));

  if (secure_hash_)
    secure_hash_->Update(data, data_len);

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

bool BaseFile::ValidateDataInFile(int64_t offset,
                                  const char* data,
                                  size_t data_len) {
  if (!file_.IsValid())
    return false;

  // Only validate the first chunk of the file. So |offset| cannot be
  // larger than bytes received.
  if (offset > bytes_so_far_)
    return false;

  if (data_len <= 0)
    return true;

  std::unique_ptr<char[]> buffer(new char[data_len]);
  if (file_.Read(offset, buffer.get(), data_len) <= 0)
    return false;

  return memcmp(data, buffer.get(), data_len) == 0;
}

DownloadInterruptReason BaseFile::Rename(const base::FilePath& new_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DownloadInterruptReason rename_result = DOWNLOAD_INTERRUPT_REASON_NONE;

  // If the new path is same as the old one, there is no need to perform the
  // following renaming logic.
  if (new_path == full_path_)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  // Save the information whether the download is in progress because
  // it will be overwritten by closing the file.
  bool was_in_progress = in_progress();

  Close();

  CONDITIONAL_TRACE(BEGIN2("download", "DownloadFileRename", "old_filename",
                           full_path_.AsUTF8Unsafe(), "new_filename",
                           new_path.AsUTF8Unsafe()));
  bool need_to_move_file = true;
#if defined(OS_ANDROID)
  if (new_path.IsContentUri()) {
    rename_result = DownloadCollectionBridge::MoveFileToIntermediateUri(
        full_path_, new_path);
    need_to_move_file = false;
  }
#endif
  if (need_to_move_file) {
    base::CreateDirectory(new_path.DirName());

    // A simple rename wouldn't work here since we want the file to have
    // permissions / security descriptors that makes sense in the new directory.
    rename_result = MoveFileAndAdjustPermissions(new_path);
  }

  CONDITIONAL_TRACE(END0("download", "DownloadFileRename"));

  if (rename_result == DOWNLOAD_INTERRUPT_REASON_NONE)
    full_path_ = new_path;

  // Re-open the file if we were still using it regardless of the interrupt
  // reason.
  DownloadInterruptReason open_result = DOWNLOAD_INTERRUPT_REASON_NONE;
  if (was_in_progress) {
    int64_t bytes_wasted;  // Do not need to use bytes_wasted.
    open_result = Open(std::string(), &bytes_wasted);
  }

  return rename_result == DOWNLOAD_INTERRUPT_REASON_NONE ? open_result
                                                         : rename_result;
}

void BaseFile::Detach() {
  weak_factory_.InvalidateWeakPtrs();
  detached_ = true;
  CONDITIONAL_TRACE(
      INSTANT0("download", "DownloadFileDetached", TRACE_EVENT_SCOPE_THREAD));
}

void BaseFile::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!detached_);

  CONDITIONAL_TRACE(
      INSTANT0("download", "DownloadCancelled", TRACE_EVENT_SCOPE_THREAD));

  Close();

  if (!full_path_.empty()) {
    CONDITIONAL_TRACE(
        INSTANT0("download", "DownloadFileDeleted", TRACE_EVENT_SCOPE_THREAD));
    DeleteFile(full_path_);
  }

  Detach();
}

std::unique_ptr<crypto::SecureHash> BaseFile::Finish() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(qinmin): verify that all the holes have been filled.
  if (is_sparse_file_)
    CalculatePartialHash(std::string());
  Close();
  return std::move(secure_hash_);
}

std::string BaseFile::DebugString() const {
  return base::StringPrintf(
      "{ "
      " full_path_ = \"%" PRFilePath
      "\""
      " bytes_so_far_ = %" PRId64 " detached_ = %c }",
      full_path_.value().c_str(), bytes_so_far_, detached_ ? 'T' : 'F');
}

DownloadInterruptReason BaseFile::CalculatePartialHash(
    const std::string& hash_to_expect) {
  secure_hash_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);

  if (bytes_so_far_ == 0)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  if (file_.Seek(base::File::FROM_BEGIN, 0) != 0)
    return LogSystemError("Seek partial file",
                          logging::GetLastSystemErrorCode());

  const size_t kMinBufferSize = secure_hash_->GetHashLength();
  const size_t kMaxBufferSize = 1024 * 512;
  static_assert(kMaxBufferSize <= std::numeric_limits<int>::max(),
                "kMaxBufferSize must fit on an int");

  // The size of the buffer is:
  // - at least kMinBufferSize so that we can use it to hold the hash as well.
  // - at most kMaxBufferSize so that there's a reasonable bound.
  // - not larger than |bytes_so_far_| unless bytes_so_far_ is less than the
  //   hash size.
  std::vector<char> buffer(std::max<int64_t>(
      kMinBufferSize, std::min<int64_t>(kMaxBufferSize, bytes_so_far_)));

  int64_t current_position = 0;
  while (current_position < bytes_so_far_) {
    // While std::min needs to work with int64_t, the result is always at most
    // kMaxBufferSize, which fits on an int.
    int bytes_to_read =
        std::min<int64_t>(buffer.size(), bytes_so_far_ - current_position);
    int length = file_.ReadAtCurrentPos(&buffer.front(), bytes_to_read);
    if (length == -1) {
      return LogInterruptReason("Reading partial file",
                                logging::GetLastSystemErrorCode(),
                                DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
    }

    if (length == 0)
      break;

    secure_hash_->Update(&buffer.front(), length);
    current_position += length;
  }

  if (current_position != bytes_so_far_) {
    return LogInterruptReason("Verifying prefix hash", 0,
                              DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
  }

  if (!hash_to_expect.empty()) {
    DCHECK_EQ(secure_hash_->GetHashLength(), hash_to_expect.size());
    DCHECK(buffer.size() >= secure_hash_->GetHashLength());
    std::unique_ptr<crypto::SecureHash> partial_hash(secure_hash_->Clone());
    partial_hash->Finish(&buffer.front(), buffer.size());

    if (memcmp(&buffer.front(), hash_to_expect.c_str(),
               partial_hash->GetHashLength())) {
      return LogInterruptReason("Verifying prefix hash", 0,
                                DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH);
    }
  }

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

DownloadInterruptReason BaseFile::Open(const std::string& hash_so_far,
                                       int64_t* const bytes_wasted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!detached_);
  DCHECK(!full_path_.empty());

  // Create a new file if it is not provided.
  if (!file_.IsValid()) {
    InitializeFile(&file_, full_path_);
    if (!file_.IsValid()) {
      return LogNetError("Open/Initialize File",
                         net::FileErrorToNetError(file_.error_details()));
    }
  }

  CONDITIONAL_TRACE(NESTABLE_ASYNC_BEGIN2(
      "download", "DownloadFileOpen", download_id_, "file_name",
      full_path_.AsUTF8Unsafe(), "bytes_so_far", bytes_so_far_));

  // For sparse file, skip hash validation.
  if (is_sparse_file_) {
    if (file_.GetLength() < bytes_so_far_) {
      *bytes_wasted = bytes_so_far_;
      ClearFile();
      return LogInterruptReason("File has fewer written bytes than expected", 0,
                                DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
    }
    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  if (!secure_hash_) {
    DownloadInterruptReason reason = CalculatePartialHash(hash_so_far);
    if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
      *bytes_wasted = file_.GetLength();
      ClearFile();
      return reason;
    }
  }

  int64_t file_size = file_.Seek(base::File::FROM_END, 0);
  if (file_size < 0) {
    logging::SystemErrorCode error = logging::GetLastSystemErrorCode();
    ClearFile();
    return LogSystemError("Seeking to end", error);
  } else if (file_size > bytes_so_far_) {
    // The file is larger than we expected.
    // This is OK, as long as we don't use the extra.
    // Truncate the file.
    *bytes_wasted = file_size - bytes_so_far_;
    if (!file_.SetLength(bytes_so_far_) ||
        file_.Seek(base::File::FROM_BEGIN, bytes_so_far_) != bytes_so_far_) {
      logging::SystemErrorCode error = logging::GetLastSystemErrorCode();
      *bytes_wasted = file_size;
      ClearFile();
      return LogSystemError("Truncating to last known offset", error);
    }
  } else if (file_size < bytes_so_far_) {
    // The file is shorter than we expected.  Our hashes won't be valid.
    *bytes_wasted = bytes_so_far_;
    ClearFile();
    return LogInterruptReason("Unable to seek to last written point", 0,
                              DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
  }

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

void BaseFile::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (file_.IsValid()) {
    // Currently we don't really care about the return value, since if it fails
    // theres not much we can do.  But we might in the future.
    file_.Flush();
    ClearFile();
  }
}

void BaseFile::ClearFile() {
  // This should only be called when we have a stream.
  DCHECK(file_.IsValid());
  file_.Close();
  CONDITIONAL_TRACE(
      NESTABLE_ASYNC_END0("download", "DownloadFileOpen", download_id_));
}

DownloadInterruptReason BaseFile::LogNetError(const char* operation,
                                              net::Error error) {
  CONDITIONAL_TRACE(INSTANT2("download", "DownloadFileError",
                             TRACE_EVENT_SCOPE_THREAD, "operation", operation,
                             "net_error", error));
  return ConvertNetErrorToInterruptReason(error, DOWNLOAD_INTERRUPT_FROM_DISK);
}

DownloadInterruptReason BaseFile::LogSystemError(
    const char* operation,
    logging::SystemErrorCode os_error) {
  // There's no direct conversion from a system error to an interrupt reason.
  base::File::Error file_error = base::File::OSErrorToFileError(os_error);
  return LogInterruptReason(operation, os_error,
                            ConvertFileErrorToInterruptReason(file_error));
}

DownloadInterruptReason BaseFile::LogInterruptReason(
    const char* operation,
    int os_error,
    DownloadInterruptReason reason) {
  DVLOG(1) << __func__ << "() operation:" << operation
           << " os_error:" << os_error
           << " reason:" << DownloadInterruptReasonToString(reason);
  auto error_data =
      std::make_unique<FileErrorData>(operation, os_error, reason);
  CONDITIONAL_TRACE(INSTANT1("download", "DownloadFileError",
                             TRACE_EVENT_SCOPE_THREAD, "file_error",
                             std::move(error_data)));
  return reason;
}

#if defined(OS_ANDROID)
DownloadInterruptReason BaseFile::PublishDownload() {
  Close();
  base::FilePath new_path =
      DownloadCollectionBridge::PublishDownload(full_path_);
  if (!new_path.empty()) {
    full_path_ = new_path;
    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }
  return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
}
#endif  // defined(OS_ANDROID)

namespace {

DownloadInterruptReason QuarantineFileResultToReason(
    quarantine::mojom::QuarantineFileResult result) {
  switch (result) {
    case quarantine::mojom::QuarantineFileResult::OK:
      return DOWNLOAD_INTERRUPT_REASON_NONE;
    case quarantine::mojom::QuarantineFileResult::VIRUS_INFECTED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED;
    case quarantine::mojom::QuarantineFileResult::SECURITY_CHECK_FAILED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED;
    case quarantine::mojom::QuarantineFileResult::BLOCKED_BY_POLICY:
      return DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED;
    case quarantine::mojom::QuarantineFileResult::ACCESS_DENIED:
      return DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;

    case quarantine::mojom::QuarantineFileResult::FILE_MISSING:
      // Don't have a good interrupt reason here. This return code means that
      // the file at |full_path_| went missing before QuarantineFile got to
      // look at it. Not expected to happen, but we've seen instances where a
      // file goes missing immediately after BaseFile closes the handle.
      //
      // Intentionally using a different error message than
      // SECURITY_CHECK_FAILED in order to distinguish the two.
      return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;

    case quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED:
      // This means that the mark-of-the-web couldn't be applied. The file is
      // already on the file system under its final target name.
      //
      // Causes of failed annotations typically aren't transient. E.g. the
      // target file system may not support extended attributes or alternate
      // streams. We are going to allow these downloads to progress on the
      // assumption that failures to apply MOTW can't reliably be introduced
      // remotely.
      return DOWNLOAD_INTERRUPT_REASON_NONE;
  }
  return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
}

// Given a source and a referrer, determines the "safest" URL that can be used
// to determine the authority of the download source. Returns an empty URL if no
// HTTP/S URL can be determined for the <|source_url|, |referrer_url|> pair.
GURL GetEffectiveAuthorityURL(const GURL& source_url,
                              const GURL& referrer_url) {
  if (source_url.is_valid()) {
    // http{,s} has an authority and are supported.
    if (source_url.SchemeIsHTTPOrHTTPS())
      return source_url;

    // If the download source is file:// ideally we should copy the MOTW from
    // the original file, but given that Chrome/Chromium places strict
    // restrictions on which schemes can reference file:// URLs, this code is
    // going to assume that at this point it's okay to treat this download as
    // being from the local system.
    if (source_url.SchemeIsFile())
      return source_url;

    // ftp:// has an authority.
    if (source_url.SchemeIs(url::kFtpScheme))
      return source_url;
  }

  if (referrer_url.is_valid() && referrer_url.SchemeIsHTTPOrHTTPS())
    return referrer_url;

  return GURL();
}

}  // namespace

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

DownloadInterruptReason BaseFile::AnnotateWithSourceInformationSync(
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!detached_);
  DCHECK(!full_path_.empty());

  CONDITIONAL_TRACE(BEGIN0("download", "DownloadFileAnnotate"));
  QuarantineFileResult result = QuarantineFile(
      full_path_, GetEffectiveAuthorityURL(source_url, referrer_url),
      referrer_url, client_guid);
  CONDITIONAL_TRACE(END0("download", "DownloadFileAnnotate"));

  return QuarantineFileResultToReason(result);
}
#else  // !OS_WIN && !OS_MACOSX && !OS_LINUX
DownloadInterruptReason BaseFile::AnnotateWithSourceInformationSync(
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url) {
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}
#endif

void BaseFile::OnFileQuarantined(
    bool connection_error,
    quarantine::mojom::QuarantineFileResult result) {
  base::UmaHistogramBoolean("Download.QuarantineService.ConnectionError",
                            connection_error);

  DCHECK(on_annotation_done_callback_);
  quarantine_service_.reset();
  std::move(on_annotation_done_callback_)
      .Run(QuarantineFileResultToReason(result));
}

void BaseFile::OnQuarantineServiceError(const GURL& source_url,
                                        const GURL& referrer_url) {
#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(quarantine::kOutOfProcessQuarantine)) {
    OnFileQuarantined(/*connection_error=*/true,
                      quarantine::SetInternetZoneIdentifierDirectly(
                          full_path_, source_url, referrer_url));
    return;
  }
#endif  // defined(OS_WIN)

  CHECK(false) << "In-process quarantine service should not have failed.";
}

void BaseFile::AnnotateWithSourceInformation(
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    OnAnnotationDoneCallback on_annotation_done_callback) {
  GURL authority_url = GetEffectiveAuthorityURL(source_url, referrer_url);
  if (!remote_quarantine) {
#if defined(OS_WIN)
    QuarantineFileResult result = quarantine::SetInternetZoneIdentifierDirectly(
        full_path_, authority_url, referrer_url);
#else
    QuarantineFileResult result = QuarantineFileResult::ANNOTATION_FAILED;
#endif
    std::move(on_annotation_done_callback)
        .Run(QuarantineFileResultToReason(result));
  } else {
    quarantine_service_.Bind(std::move(remote_quarantine));

    on_annotation_done_callback_ = std::move(on_annotation_done_callback);

    quarantine_service_.set_disconnect_handler(base::BindOnce(
        &BaseFile::OnQuarantineServiceError, weak_factory_.GetWeakPtr(),
        authority_url, referrer_url));

    quarantine_service_->QuarantineFile(
        full_path_, authority_url, referrer_url, client_guid,
        base::BindOnce(&BaseFile::OnFileQuarantined, weak_factory_.GetWeakPtr(),
                       false));
  }
}

}  // namespace download
