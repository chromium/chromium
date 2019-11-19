// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/zip_archiver/sandboxed_zip_archiver.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/constants/quarantine_constants.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

namespace {

using mojom::ZipArchiverResultCode;

// According to the zip structure and tests, zipping one file with STORE
// compression level should not increase the file size more than 1KB.
constexpr int64_t kZipAdditionalSize = 1024;

constexpr size_t kDefaultMaxComponentLength = 255;

constexpr wchar_t kDefaultFileStreamSuffix[] = L"::$DATA";
constexpr uint32_t kMinimizedReadAccess =
    SYNCHRONIZE | FILE_READ_DATA | FILE_READ_ATTRIBUTES;
constexpr uint32_t kMinimizedWriteAccess =
    SYNCHRONIZE | FILE_WRITE_DATA | FILE_READ_ATTRIBUTES;

// NTFS file stream can be specified by appending ":" to the filename. We remove
// the default file stream "::$DATA" so it won't break the filename in the
// following uses. For other file streams, we don't archive and ignore them.
bool GetSanitizedFileName(const base::FilePath& path,
                          base::string16* output_sanitized_filename) {
  DCHECK(output_sanitized_filename);

  base::string16 sanitized_filename = path.BaseName().AsUTF16Unsafe();
  if (base::EndsWith(sanitized_filename, kDefaultFileStreamSuffix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    // Remove the default file stream suffix.
    sanitized_filename.erase(
        sanitized_filename.end() - wcslen(kDefaultFileStreamSuffix),
        sanitized_filename.end());
  }
  // If there is any ":" in |sanitized_filename|, it either points to a
  // non-default file stream or is abnormal. Don't archive in this case.
  if (sanitized_filename.find(L":") != base::string16::npos)
    return false;

  *output_sanitized_filename = sanitized_filename;
  return true;
}

void RunArchiver(mojo::Remote<mojom::ZipArchiver>* zip_archiver,
                 mojo::ScopedHandle mojo_src_handle,
                 mojo::ScopedHandle mojo_zip_handle,
                 const std::string& filename,
                 const std::string& password,
                 mojom::ZipArchiver::ArchiveCallback callback) {
  DCHECK(zip_archiver);

  (*zip_archiver)
      ->Archive(std::move(mojo_src_handle), std::move(mojo_zip_handle),
                filename, password, std::move(callback));
}

void OnArchiveDone(const base::FilePath& zip_file_path,
                   SandboxedZipArchiver::ArchiveResultCallback result_callback,
                   ZipArchiverResultCode result_code) {
  if (result_code != ZipArchiverResultCode::kSuccess) {
    // The zip file handle has been closed by mojo. Delete the incomplete zip
    // file directly.
    if (!base::DeleteFile(zip_file_path, /*recursive=*/false))
      LOG(ERROR) << "Failed to delete the incomplete zip file.";
  }
  // Call |result_callback| for SandboxedZipArchiver::Archive.
  std::move(result_callback).Run(result_code);
}

}  // namespace

namespace internal {

// Zip file name format: "|filename|_|file_hash|.zip"
base::string16 ConstructZipArchiveFileName(const base::string16& filename,
                                           const std::string& file_hash,
                                           size_t max_filename_length) {
  const base::string16 normalized_hash =
      base::UTF8ToUTF16(base::ToUpperASCII(file_hash));

  // Length of the ".zip" suffix and the "_" infix.
  constexpr size_t kAffixSize = 5;

  // If the constructed filename is too long for the destination volume, use a
  // prefix of the filename.
  base::string16 normalized_filename;
  if (filename.size() + normalized_hash.size() + kAffixSize >
      max_filename_length) {
    size_t trimmed_length =
        max_filename_length - normalized_hash.size() - kAffixSize;
    normalized_filename = filename.substr(0, trimmed_length);
  } else {
    normalized_filename = filename;
  }

  base::string16 result =
      base::StrCat({normalized_filename, L"_", normalized_hash, L".zip"});
  DCHECK(result.size() <= max_filename_length);
  DCHECK(result.size() ==
         normalized_filename.size() + normalized_hash.size() + kAffixSize);
  return result;
}

}  // namespace internal

SandboxedZipArchiver::SandboxedZipArchiver(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    RemoteZipArchiverPtr zip_archiver,
    const base::FilePath& dst_archive_folder,
    const std::string& zip_password)
    : mojo_task_runner_(mojo_task_runner),
      zip_archiver_(std::move(zip_archiver)),
      dst_archive_folder_(dst_archive_folder),
      zip_password_(zip_password) {
  // Make sure the |zip_archiver| is bound with the |mojo_task_runner|.
  DCHECK(zip_archiver_.get_deleter().task_runner_ == mojo_task_runner);

  int max_component_length =
      base::GetMaximumPathComponentLength(dst_archive_folder_);
  dst_max_component_length_ = max_component_length > 0
                                  ? max_component_length
                                  : kDefaultMaxComponentLength;
}

SandboxedZipArchiver::~SandboxedZipArchiver() = default;

// |SandboxedZipArchiver::Archive| archives the source file into a
// password-protected zip file stored in the |dst_archive_folder|. The format of
// zip file name is "|basename of the source file|_|hexdigest of the source file
// hash|.zip".
void SandboxedZipArchiver::Archive(const base::FilePath& src_file_path,
                                   ArchiveResultCallback result_callback) {
  // Open the source file with minimized rights for reading.
  // Allowing all sharing accesses increases the chances of being able to open
  // and archive the file. Because |base::IsLink| doesn't work on Windows, use
  // |FILE_FLAG_OPEN_REPARSE_POINT| to open a symbolic link then check. To
  // eliminate TOCTTOU, use |FILE_FLAG_BACKUP_SEMANTICS| to open a directory
  // then check.
  base::File src_file(::CreateFile(
      src_file_path.AsUTF16Unsafe().c_str(), kMinimizedReadAccess,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
      nullptr));
  if (!src_file.IsValid()) {
    LOG(ERROR) << "Unable to open the source file.";
    std::move(result_callback)
        .Run(ZipArchiverResultCode::kErrorCannotOpenSourceFile);
    return;
  }

  BY_HANDLE_FILE_INFORMATION src_file_info;
  if (!::GetFileInformationByHandle(src_file.GetPlatformFile(),
                                    &src_file_info)) {
    LOG(ERROR) << "Unable to get the source file information.";
    std::move(result_callback).Run(ZipArchiverResultCode::kErrorIO);
    return;
  }

  // Don't archive symbolic links.
  if (src_file_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    std::move(result_callback).Run(ZipArchiverResultCode::kIgnoredSourceFile);
    return;
  }

  // Don't archive directories. And |ZipArchiver| shouldn't get called with a
  // directory path.
  if (src_file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    LOG(ERROR) << "Tried to archive a directory.";
    std::move(result_callback).Run(ZipArchiverResultCode::kIgnoredSourceFile);
    return;
  }

  base::string16 sanitized_src_filename;
  if (!GetSanitizedFileName(src_file_path, &sanitized_src_filename)) {
    std::move(result_callback).Run(ZipArchiverResultCode::kIgnoredSourceFile);
    return;
  }

  const ZipArchiverResultCode result_code = CheckFileSize(&src_file);
  if (result_code != ZipArchiverResultCode::kSuccess) {
    std::move(result_callback).Run(result_code);
    return;
  }

  std::string src_file_hash;
  if (!ComputeSHA256DigestOfPath(src_file_path, &src_file_hash)) {
    LOG(ERROR) << "Unable to hash the source file.";
    std::move(result_callback).Run(ZipArchiverResultCode::kErrorIO);
    return;
  }

  const base::string16 zip_filename = internal::ConstructZipArchiveFileName(
      sanitized_src_filename, src_file_hash, dst_max_component_length_);
  base::FilePath zip_file_path = dst_archive_folder_.Append(zip_filename);

  // If the full path is longer than MAX_PATH, prepending "\\?\" allows to
  // extend the path limit (see CreateFile documentation).
  if (zip_file_path.value().size() >= MAX_PATH) {
    base::string16 long_file_path =
        base::string16(L"\\\\?\\") + zip_file_path.value();
    zip_file_path = base::FilePath(long_file_path);
  }

  // Fail if the zip file exists.
  if (base::PathExists(zip_file_path)) {
    std::move(result_callback).Run(ZipArchiverResultCode::kZipFileExists);
    return;
  }

  // Create and open the zip file with minimized rights for writing.
  base::File zip_file(::CreateFile(zip_file_path.AsUTF16Unsafe().c_str(),
                                   kMinimizedWriteAccess, 0, nullptr,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!zip_file.IsValid()) {
    PLOG(ERROR) << "Unable to create the zip file at path "
                << SanitizePath(zip_file_path);
    std::move(result_callback)
        .Run(ZipArchiverResultCode::kErrorCannotCreateZipFile);
    return;
  }

  const std::string filename_in_zip = base::UTF16ToUTF8(sanitized_src_filename);
  // Do archive.
  // Unretained pointer of |zip_archiver_| is safe because its deleter is run on
  // the same task runner. If |zip_archiver_| is destructed later, the deleter
  // will be scheduled after this task.
  auto done_callback =
      base::BindOnce(OnArchiveDone, zip_file_path, std::move(result_callback));
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(RunArchiver, base::Unretained(zip_archiver_.get()),
                     mojo::WrapPlatformFile(src_file.TakePlatformFile()),
                     mojo::WrapPlatformFile(zip_file.TakePlatformFile()),
                     filename_in_zip, zip_password_, std::move(done_callback)));
}

ZipArchiverResultCode SandboxedZipArchiver::CheckFileSize(base::File* file) {
  const int64_t file_size = file->GetLength();
  if (file_size == -1) {
    LOG(ERROR) << "Unable to get the file size.";
    return ZipArchiverResultCode::kErrorIO;
  }
  if (file_size > kQuarantineSourceSizeLimit) {
    LOG(ERROR) << "Source file is too big.";
    return ZipArchiverResultCode::kErrorSourceFileTooBig;
  }

  const int64_t dst_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(dst_archive_folder_);
  if (dst_disk_space == -1) {
    LOG(ERROR) << "Unable to get the free disk space.";
    return ZipArchiverResultCode::kErrorIO;
  }
  if (file_size + kZipAdditionalSize > dst_disk_space) {
    LOG(ERROR) << "Not enough disk space.";
    return ZipArchiverResultCode::kErrorNotEnoughDiskSpace;
  }

  return ZipArchiverResultCode::kSuccess;
}

ResultCode SpawnZipArchiverSandbox(
    const base::FilePath& dst_archive_folder,
    const std::string& zip_password,
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    std::unique_ptr<SandboxedZipArchiver>* sandboxed_zip_archiver) {
  DCHECK(sandboxed_zip_archiver);

  auto error_handler =
      base::BindOnce(connection_error_callback, SandboxType::kZipArchiver);
  ZipArchiverSandboxSetupHooks setup_hooks(mojo_task_runner,
                                           std::move(error_handler));
  ResultCode result_code =
      SpawnSandbox(&setup_hooks, SandboxType::kZipArchiver);
  if (result_code == RESULT_CODE_SUCCESS) {
    *sandboxed_zip_archiver = std::make_unique<SandboxedZipArchiver>(
        mojo_task_runner, setup_hooks.TakeZipArchiverRemote(),
        dst_archive_folder, zip_password);
  }

  return result_code;
}

}  // namespace chrome_cleaner
