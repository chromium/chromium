// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"

#include <algorithm>
#include <string_view>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kReadFileChunkSize = 4096;
constexpr size_t kMaxUploadSizeMetricsKB = 500 * 1024;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
bool IsZipFile(const base::FilePath::StringType& extension,
               const std::string& mime_type) {
  return extension == FILE_PATH_LITERAL(".zip") ||
         mime_type == "application/x-zip-compressed" ||
         mime_type == "application/zip";
}

bool IsRarFile(const base::FilePath::StringType& extension,
               const std::string& mime_type) {
  return extension == FILE_PATH_LITERAL(".rar") ||
         mime_type == "application/vnd.rar" ||
         mime_type == "application/x-rar-compressed";
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

std::string GetFileMimeType(const base::FilePath& path,
                            std::string_view first_bytes) {
  std::string sniffed_mime_type;
  bool sniff_found = net::SniffMimeType(
      std::string_view(first_bytes.data(),
                       std::min(first_bytes.size(),
                                static_cast<size_t>(net::kMaxBytesToSniff))),
      net::FilePathToFileURL(path),
      /*type_hint*/ std::string(), net::ForceSniffFileUrlsForHtml::kDisabled,
      &sniffed_mime_type);

  if (sniff_found && !sniffed_mime_type.empty() &&
      sniffed_mime_type != "text/*" &&
      sniffed_mime_type != "application/octet-stream") {
    return sniffed_mime_type;
  }

  // If the file got a trivial or empty mime type sniff, fall back to using its
  // extension if possible.
  base::FilePath::StringType ext = path.FinalExtension();
  if (ext.empty()) {
    return sniffed_mime_type;
  }

  if (ext[0] == FILE_PATH_LITERAL('.')) {
    ext = ext.substr(1);
  }

  std::string ext_mime_type;
  bool ext_found = net::GetMimeTypeFromExtension(ext, &ext_mime_type);

  if (!ext_found || ext_mime_type.empty()) {
    return sniffed_mime_type;
  }

  return ext_mime_type;
}

// Perform hash computation, which can be CPU cycle intensive. Since hash
// computation can occur after RunCallback to unblock the user faster, make it
// the last operation which uses the file and release the associated scoped file
// access at the end of this function.
std::string ComputeHashBlocking(
    base::File file,
    std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access) {
  if (file.Seek(base::File::FROM_BEGIN, 0) != 0) {
    return "";  // return empty string to indicate error.
  }

  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  size_t bytes_read = 0;
  size_t file_size = file.GetLength();
  std::vector<char> buf(kReadFileChunkSize);

  while (bytes_read < file_size) {
    std::optional<size_t> bytes_currently_read =
        file.ReadAtCurrentPos(base::as_writable_byte_span(buf));
    if (!bytes_currently_read.has_value()) {
      return "";  // return empty string to indicate error.
    }

    secure_hash->Update(
        base::as_byte_span(buf).first(bytes_currently_read.value()));
    bytes_read += bytes_currently_read.value();
  }

  std::array<uint8_t, crypto::kSHA256Length> hash;
  secure_hash->Finish(hash);

  return base::HexEncode(hash);
}

std::pair<ScanRequestUploadResult, BinaryUploadRequest::Data>
GetFileDataBlocking(
    const base::FilePath& path,
    std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access,
    bool detect_mime_type,
    bool is_obfuscated,
    bool force_sync_hash_computation,
    base::OnceCallback<std::string()>& output_compute_hash_callback) {
  DCHECK(!path.empty());

  // The returned `Data` must always have a valid `path` member, regardless
  // if this function succeeds or not.  The other members of `Data` may or
  // may not be filled in.
  BinaryUploadRequest::Data file_data;
  file_data.path = path;

  // FLAG_WIN_SHARE_DELETE is necessary to allow the file to be renamed by the
  // user clicking "Open Now" without causing download errors.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);

  if (!file.IsValid()) {
    return std::make_pair(ScanRequestUploadResult::kUnknown, file_data);
  }

  file_data.size = file.GetLength();
  if (file_data.size == 0) {
    return std::make_pair(ScanRequestUploadResult::kSuccess, file_data);
  }

  std::vector<char> buf(kReadFileChunkSize);

  std::optional<size_t> bytes_currently_read =
      file.ReadAtCurrentPos(base::as_writable_byte_span(buf));
  if (!bytes_currently_read.has_value()) {
    // Reset the size to zero since some code assumes an UNKNOWN result is
    // matched with a zero size.
    file_data.size = 0;
    return {ScanRequestUploadResult::kUnknown, file_data};
  }

  // Use the first read chunk to get the mimetype as necessary.
  if (detect_mime_type) {
    file_data.mime_type = GetFileMimeType(
        path, std::string_view(buf.data(), bytes_currently_read.value()));
  }

  // Since we will be sending the deobfuscated file data in the request, set the
  // size to match.
  if (is_obfuscated) {
    enterprise_obfuscation::DownloadObfuscator obfuscator;
    auto overhead = obfuscator.CalculateDeobfuscationOverhead(file);
    if (overhead.has_value()) {
      file_data.size -= overhead.value();
      file_data.is_obfuscated = true;
    }
  }

  // Create a histogram to track the size of files being scanned up to 500MB.
  base::UmaHistogramCustomCounts("Enterprise.FileAnalysisRequestBase.FileSize",
                                 file_data.size / 1024, 1,
                                 kMaxUploadSizeMetricsKB, 50);

  // When forced, or if the feature is not enabled, or the file is not large
  // enough, compute the hash now and add to file data.
  if (force_sync_hash_computation ||
      !base::FeatureList::IsEnabled(
          enterprise_connectors::kContentHashInFileUploadFinalCall) ||
      file_data.size <=
          enterprise_connectors::BinaryUploadService::kMaxUploadSizeBytes) {
    // TODO(crbug.com/367257039): Pass along hash of unobfuscated file for
    // enterprise scans
    file_data.hash =
        ComputeHashBlocking(std::move(file), std::move(scoped_file_access));
    if (file_data.hash.empty()) {
      // Reset the size to zero since some code assumes an UNKNOWN result is
      // matched with a zero size.
      file_data.size = 0;
      return {enterprise_connectors::ScanRequestUploadResult::kUnknown,
              file_data};
    }
  } else {
    // When all three conditions are false, set the function parameter reference
    // to a callback that computes the hash.
    output_compute_hash_callback = base::BindOnce(
        &ComputeHashBlocking, std::move(file), std::move(scoped_file_access));
  }

  size_t max_file_size_bytes = BinaryUploadService::kMaxUploadSizeBytes;
  if (base::FeatureList::IsEnabled(kEnableNewUploadSizeLimit)) {
    max_file_size_bytes = 1024 * 1024 * kMaxContentAnalysisFileSizeMB.Get();
  }
  return {file_data.size <= max_file_size_bytes
              ? ScanRequestUploadResult::kSuccess
              : ScanRequestUploadResult::kFileTooLarge,
          std::move(file_data)};
}

}  // namespace

FileAnalysisRequestBase::FileAnalysisRequestBase(
    const AnalysisSettings& analysis_settings,
    base::FilePath path,
    base::FilePath file_name,
    std::string mime_type,
    bool delay_opening_file,
    BinaryUploadRequest::ContentAnalysisCallback callback,
    BrowserPolicyConnectorGetter policy_connector_getter,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    BinaryUploadRequest::RequestStartCallback start_callback,
    bool is_obfuscated,
    bool force_sync_hash_computation)
    : BinaryUploadRequest(std::move(callback),
                          analysis_settings.cloud_or_local_settings,
                          std::move(start_callback),
                          std::move(policy_connector_getter)),
      tag_settings_(analysis_settings.tags),
      path_(std::move(path)),
      file_name_(std::move(file_name)),
      delay_opening_file_(delay_opening_file),
      is_obfuscated_(is_obfuscated),
      force_sync_hash_computation_(force_sync_hash_computation),
      ui_task_runner_(ui_task_runner) {
  CHECK(ui_task_runner_);
  DCHECK(!path_.empty());
  set_filename(path_.AsUTF8Unsafe());
  cached_data_.mime_type = std::move(mime_type);
}

FileAnalysisRequestBase::~FileAnalysisRequestBase() {
  // If the object is going to be gone but there are still callbacks waiting for
  // hash, let them know some error occurred.
  if (!hash_notify_callbacks_.empty()) {
    OnGotHash(std::string());
  }
}

void FileAnalysisRequestBase::GetRequestData(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_callback_ = std::move(callback);

  if (has_cached_result_) {
    RunCallback();
    return;
  }

  if (!delay_opening_file_) {
    file_access::RequestFilesAccessForSystem(
        {path_}, base::BindOnce(&FileAnalysisRequestBase::GetData,
                                weakptr_factory_.GetWeakPtr()));
  }
}

void FileAnalysisRequestBase::OpenFile() {
  DCHECK(!data_callback_.is_null());

  // Opening the file synchronously here is OK since OpenFile should be called
  // on a base::MayBlock() thread.
  base::OnceCallback<std::string()> compute_hash_callback;
  std::pair<enterprise_connectors::ScanRequestUploadResult, Data> file_data =
      GetFileDataBlocking(path_, std::move(scoped_file_access_),
                          cached_data_.mime_type.empty(), is_obfuscated_,
                          force_sync_hash_computation_, compute_hash_callback);

  if (compute_hash_callback) {
    register_on_got_hash_callback_ = base::BindPostTask(
        ui_task_runner_,
        base::BindRepeating(&FileAnalysisRequestBase::RegisterOnGotHashCallback,
                            weakptr_factory_.GetWeakPtr()));
  }

  // The result of opening the file is passed back to the UI thread since
  // |data_callback_| calls functions that must run there.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FileAnalysisRequestBase::OnGotFileData,
                     weakptr_factory_.GetWeakPtr(), std::move(file_data)));

  if (compute_hash_callback) {
    std::move(compute_hash_callback)
        .Then(base::BindPostTask(
            ui_task_runner_, base::BindOnce(&FileAnalysisRequestBase::OnGotHash,
                                            weakptr_factory_.GetWeakPtr())))
        .Run();
  }
}

void FileAnalysisRequestBase::RegisterOnGotHashCallback(
    bool call_last,
    enterprise_connectors::OnGotHashCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!cached_data_.hash.empty() ||
      (cached_result_ ==
       enterprise_connectors::ScanRequestUploadResult::kUnknown)) {
    std::move(callback).Run(cached_data_.hash);
  } else {
    // TODO(alxchn): Test that call_last will only be ever called once through
    // an upload.
    if (call_last) {
      hash_notify_callbacks_.push_back(std::move(callback));
    } else {
      hash_notify_callbacks_.push_front(std::move(callback));
    }
  }
}

void FileAnalysisRequestBase::OnGotHash(std::string hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (hash.empty()) {
    // If the returned hash was empty, an error must have occurred while
    // computing. Reset the size to zero since some code assumes an kUnknown
    // result is matched with a zero size. Registered callbacks should also
    // handle this.
    cached_data_.size = 0;
    cached_result_ = enterprise_connectors::ScanRequestUploadResult::kUnknown;
  }
  cached_data_.hash = hash;
  set_digest(hash);
  while (!hash_notify_callbacks_.empty()) {
    std::move(hash_notify_callbacks_.front()).Run(hash);
    hash_notify_callbacks_.pop_front();
  }
}

bool FileAnalysisRequestBase::HasMalwareRequest() const {
  for (const std::string& tag : content_analysis_request().tags()) {
    if (tag == kMalwareTag) {
      return true;
    }
  }
  return false;
}

void FileAnalysisRequestBase::OnGotFileData(
    std::pair<ScanRequestUploadResult, Data> result_and_data) {
  DCHECK(!result_and_data.second.path.empty());
  DCHECK_EQ(result_and_data.second.path, path_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_file_access_.reset();
  if (result_and_data.first != ScanRequestUploadResult::kSuccess) {
    CacheResultAndData(result_and_data.first,
                       std::move(result_and_data.second));
    RunCallback();
    return;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  CacheResultAndData(enterprise_connectors::ScanRequestUploadResult::kSuccess,
                     std::move(result_and_data.second));
  RunCallback();
#else
  const std::string& mime_type = cached_data_.mime_type.empty()
                                     ? result_and_data.second.mime_type
                                     : cached_data_.mime_type;
  base::FilePath::StringType ext(file_name_.FinalExtension());
  std::ranges::transform(ext, ext.begin(), tolower);

  if (IsZipFile(ext, mime_type)) {
    ProcessZipFile(std::move(result_and_data.second));
  } else if (IsRarFile(ext, mime_type)) {
    ProcessRarFile(std::move(result_and_data.second));
  } else {
    CacheResultAndData(ScanRequestUploadResult::kSuccess,
                       std::move(result_and_data.second));
    RunCallback();
  }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

void FileAnalysisRequestBase::CacheResultAndData(ScanRequestUploadResult result,
                                                 Data data) {
  has_cached_result_ = true;
  cached_result_ = result;

  // If the mime type is already set, it shouldn't be overwritten.
  if (!cached_data_.mime_type.empty()) {
    data.mime_type = std::move(cached_data_.mime_type);
  }

  DCHECK(!data.path.empty());
  cached_data_ = std::move(data);

  set_file_size(cached_data_.size);
  set_digest(cached_data_.hash);
  set_content_type(cached_data_.mime_type);
}

void FileAnalysisRequestBase::RunCallback() {
  if (!data_callback_.is_null()) {
    std::move(data_callback_).Run(cached_result_, cached_data_);
  }
}

void FileAnalysisRequestBase::GetData(
    file_access::ScopedFileAccess file_access) {
  scoped_file_access_ =
      std::make_unique<file_access::ScopedFileAccess>(std::move(file_access));
  base::OnceCallback<std::string()> unused_hash_callback;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileDataBlocking, path_,
                     std::move(scoped_file_access_),
                     cached_data_.mime_type.empty(), is_obfuscated_,
                     force_sync_hash_computation_,
                     base::OwnedRef(std::move(unused_hash_callback))),
      base::BindOnce(&FileAnalysisRequestBase::OnGotFileData,
                     weakptr_factory_.GetWeakPtr()));
}

}  // namespace enterprise_connectors
