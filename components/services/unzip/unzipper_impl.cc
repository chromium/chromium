// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/unzip/unzipper_impl.h"

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"
#include "third_party/zlib/google/redact.h"
#include "third_party/zlib/google/zip.h"
#include "third_party/zlib/google/zip_reader.h"

namespace unzip {
namespace {

bool CreateDirectory(storage::mojom::Directory* output_dir,
                     const base::FilePath& path) {
  base::File::Error error = base::File::FILE_ERROR_IO;
  output_dir->CreateDirectory(path, &error);
  return error == base::File::FILE_OK;
}

// A file writer that uses a storage::FilesystemProxy.
class Writer : public zip::FileWriterDelegate {
 public:
  Writer(storage::mojom::Directory* output_dir, base::FilePath path)
      : FileWriterDelegate(base::File()),
        output_dir_(output_dir),
        path_(std::move(path)) {
    DCHECK(output_dir_);
  }

  // Creates the output file.
  bool PrepareOutput() override {
    base::File::Error error = base::File::FILE_ERROR_IO;
    output_dir_->OpenFile(
        path_, storage::mojom::FileOpenMode::kCreateAndOpenOnlyIfNotExists,
        storage::mojom::FileReadAccess::kReadAllowed,
        storage::mojom::FileWriteAccess::kWriteAllowed, &error, &owned_file_);
    if (error != base::File::FILE_OK) {
      LOG(ERROR) << "Cannot create file to extract " << zip::Redact(path_)
                 << ": " << base::File::ErrorToString(error);
      return false;
    }

    return FileWriterDelegate::PrepareOutput();
  }

  // Deletes the output file.
  void OnError() override {
    FileWriterDelegate::OnError();
    owned_file_.Close();

    bool success = false;
    output_dir_->DeleteFile(path_, &success);
    if (!success) {
      LOG(ERROR) << "Cannot delete extracted file " << zip::Redact(path_);
    }
  }

 private:
  const mojo::Remote<storage::mojom::Directory> owned_output_dir_;
  const raw_ptr<storage::mojom::Directory> output_dir_;
  const base::FilePath path_;
};

std::unique_ptr<zip::WriterDelegate> MakeFileWriterDelegate(
    storage::mojom::Directory* output_dir,
    const base::FilePath& path) {
  if (path == path.BaseName()) {
    return std::make_unique<Writer>(output_dir, path);
  }

  base::File::Error error = base::File::FILE_ERROR_IO;
  output_dir->CreateDirectory(path.DirName(), &error);
  if (error != base::File::Error::FILE_OK) {
    return nullptr;
  }

  return std::make_unique<Writer>(output_dir, path);
}

bool Filter(const mojo::Remote<mojom::UnzipFilter>& filter,
            const base::FilePath& path) {
  bool result = false;
  filter->ShouldUnzipFile(path, &result);
  return result;
}

// Reads the given ZIP archive, and returns all the filenames concatenated
// together in one long string capped at ~100KB, without any separator, and in
// the encoding used by the ZIP archive itself. Returns an empty string if the
// ZIP cannot be read.
std::string GetRawFileNamesFromZip(const base::File& zip_file) {
  std::string result;

  // Open ZIP archive for reading.
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file.GetPlatformFile())) {
    LOG(ERROR) << "Cannot decode ZIP archive from file handle "
               << zip_file.GetPlatformFile();
    return result;
  }

  // Reserve a ~100KB buffer.
  result.reserve(100000);

  // Iterate over file entries of the ZIP archive.
  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    const std::string& path = entry->path_in_original_encoding;

    // Stop if we have enough data in |result|.
    if (path.size() > (result.capacity() - result.size())) {
      break;
    }

    // Accumulate data in |result|.
    result += path;
  }

  LOG_IF(ERROR, result.empty()) << "Cannot extract filenames from ZIP archive";
  return result;
}

}  // namespace

UnzipperImpl::UnzipperImpl() = default;

UnzipperImpl::UnzipperImpl(mojo::PendingReceiver<mojom::Unzipper> receiver)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &UnzipperImpl::OnReceiverDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

UnzipperImpl::~UnzipperImpl() = default;

Encoding GetEncoding(const base::File& zip_file) {
  // Accumulate raw filenames.
  const std::string all_names = GetRawFileNamesFromZip(zip_file);
  if (all_names.empty()) {
    return UNKNOWN_ENCODING;
  }

  // Detect encoding.
  int consumed_bytes = 0;
  bool is_reliable = false;
  const Encoding encoding = CompactEncDet::DetectEncoding(
      all_names.data(), all_names.size(), nullptr, nullptr, nullptr,
      UNKNOWN_ENCODING, UNKNOWN_LANGUAGE,
      CompactEncDet::QUERY_CORPUS,  // Plain text
      true,                         // Exclude 7-bit encodings
      &consumed_bytes, &is_reliable);

  VLOG(1) << "Detected encoding: " << MimeEncodingName(encoding) << " ("
          << encoding << "), reliable: " << is_reliable
          << ", consumed bytes: " << consumed_bytes;

  LOG_IF(ERROR, encoding == UNKNOWN_ENCODING)
      << "Cannot detect encoding of filenames in ZIP archive";

  return encoding;
}

void UnzipperImpl::Listener(const mojo::Remote<mojom::UnzipListener>& listener,
                            uint64_t bytes) {
  listener->OnProgress(bytes);
}

bool RunUnzip(base::File zip_file,
              mojo::PendingRemote<storage::mojom::Directory> output_dir_remote,
              std::string encoding_name,
              std::string password,
              mojo::PendingRemote<mojom::UnzipFilter> filter_remote,
              mojo::PendingRemote<mojom::UnzipListener> listener_remote) {
  mojo::Remote<storage::mojom::Directory> output_dir(
      std::move(output_dir_remote));

  zip::FilterCallback filter_cb;
  if (filter_remote) {
    filter_cb = base::BindRepeating(
        &Filter, mojo::Remote<mojom::UnzipFilter>(std::move(filter_remote)));
  }

  zip::UnzipProgressCallback progress_cb;
  if (listener_remote) {
    mojo::Remote<mojom::UnzipListener> listener(std::move(listener_remote));
    progress_cb =
        base::BindRepeating(&UnzipperImpl::Listener, std::move(listener));
  }
  return zip::Unzip(
      zip_file.GetPlatformFile(),
      base::BindRepeating(&MakeFileWriterDelegate, output_dir.get()),
      base::BindRepeating(&CreateDirectory, output_dir.get()),
      {.encoding = std::move(encoding_name),
       .filter = std::move(filter_cb),
       .progress = std::move(progress_cb),
       .password = std::move(password)});
}

void UnzipperImpl::Unzip(
    base::File zip_file,
    mojo::PendingRemote<storage::mojom::Directory> output_dir_remote,
    mojom::UnzipOptionsPtr set_options,
    mojo::PendingRemote<mojom::UnzipFilter> filter_remote,
    mojo::PendingRemote<mojom::UnzipListener> listener_remote,
    UnzipCallback callback) {
  DCHECK(zip_file.IsValid());

  std::string encoding_name;
  if (set_options->encoding == "auto") {
    Encoding encoding = GetEncoding(zip_file);
    if (IsShiftJisOrVariant(encoding) || encoding == RUSSIAN_CP866) {
      encoding_name = MimeEncodingName(encoding);
    }
  } else {
    encoding_name = set_options->encoding;
  }

  runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RunUnzip, std::move(zip_file),
                     std::move(output_dir_remote), std::move(encoding_name),
                     std::move(set_options->password), std::move(filter_remote),
                     std::move(listener_remote)),
      base::BindOnce(std::move(callback)));
}

void UnzipperImpl::DetectEncoding(base::File zip_file,
                                  DetectEncodingCallback callback) {
  DCHECK(zip_file.IsValid());

  const Encoding encoding = GetEncoding(zip_file);
  std::move(callback).Run(encoding);
}

void UnzipperImpl::GetExtractedInfo(base::File zip_file,
                                    GetExtractedInfoCallback callback) {
  DCHECK(zip_file.IsValid());

  // Open ZIP archive for reading.
  zip::ZipReader reader;
  if (!reader.OpenFromPlatformFile(zip_file.GetPlatformFile())) {
    LOG(ERROR) << "Cannot decode ZIP archive from file handle "
               << zip_file.GetPlatformFile();
    unzip::mojom::InfoPtr info =
        unzip::mojom::Info::New(false, 0, false, false);
    std::move(callback).Run(std::move(info));
    return;
  }

  int64_t size = 0;
  bool valid = true;
  bool has_encrypted_content = false;
  bool uses_aes_encryption = false;

  // Iterate over file entries of the ZIP archive.
  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    // Check for (invalid) size stored.
    if (entry->original_size < 0 ||
        entry->original_size > std::numeric_limits<int64_t>::max() - size) {
      LOG(ERROR) << "ZIP bad size info from file handle "
                 << zip_file.GetPlatformFile();
      valid = false;
      break;
    }
    // Accumulate size (since original_size is signed, ignore invalid sizes).
    if (entry->original_size > 0) {
      size += entry->original_size;
    }
    if (entry->is_encrypted) {
      has_encrypted_content = true;
      if (entry->uses_aes_encryption) {
        uses_aes_encryption = true;
      }
    }
  }
  unzip::mojom::InfoPtr info = unzip::mojom::Info::New(
      valid, size, has_encrypted_content, uses_aes_encryption);
  std::move(callback).Run(std::move(info));
}

void UnzipperImpl::OnReceiverDisconnect() {
  DCHECK(receiver_.is_bound());
  receiver_.reset();
}

}  // namespace unzip
