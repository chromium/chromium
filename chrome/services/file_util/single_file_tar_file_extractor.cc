// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/single_file_tar_file_extractor.h"

#include <utility>

#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/single_file_tar_reader.h"

namespace {

// TarExtractorInner extracts a .TAR file and writes the extracted data to the
// destination file.
class TarExtractorInner : public SingleFileTarReader::Delegate {
 public:
  TarExtractorInner(
      mojo::PendingRemote<chrome::mojom::SingleFileExtractorListener>
          pending_listener,
      base::File src_file,
      base::File dst_file)
      : listener_(std::move(pending_listener)),
        src_file_(std::move(src_file)),
        dst_file_(std::move(dst_file)),
        tar_reader_(this) {}

  chrome::file_util::mojom::ExtractionResult Extract() {
    while (true) {
      if (tar_reader_.ExtractChunk() != SingleFileTarReader::Result::kSuccess)
        return tar_reader_.error();

      if (tar_reader_.IsComplete())
        return chrome::file_util::mojom::ExtractionResult::kSuccess;

      listener_->OnProgress(tar_reader_.total_bytes().value(),
                            tar_reader_.curr_bytes());
    }
  }

  void CloseFiles() {
    src_file_.Close();
    dst_file_.Close();
  }

 private:
  // SingleFileTarReader::Delegate implementation.
  // TODO(b/256967002): Use base::span<>.
  SingleFileTarReader::Result ReadTarFile(
      char* data,
      uint32_t* size,
      chrome::file_util::mojom::ExtractionResult* error) override {
    const int bytes_read = src_file_.ReadAtCurrentPos(data, *size);
    if (bytes_read < 0) {
      *error = chrome::file_util::mojom::ExtractionResult::kGenericError;
      return SingleFileTarReader::Result::kFailure;
    }
    if (bytes_read == 0) {
      // After reading the last chunk of file content, it is expected that the
      // tar_reader_.IsComplete() in the Extract function returns true and the
      // .tar.xz file extraction ends.
      *error = chrome::file_util::mojom::ExtractionResult::kGenericError;
      return SingleFileTarReader::Result::kFailure;
    }
    *size = bytes_read;
    return SingleFileTarReader::Result::kSuccess;
  }

  // SingleFileTarReader::Delegate implementation.
  // TODO(b/256967002): Use base::span<>.
  bool WriteContents(
      const char* data,
      int size,
      chrome::file_util::mojom::ExtractionResult* error) override {
    const int bytes_written = dst_file_.WriteAtCurrentPos(data, size);
    if (bytes_written < 0 || bytes_written != size) {
      *error = chrome::file_util::mojom::ExtractionResult::kDstFileError;
      return false;
    }
    return true;
  }

  const mojo::Remote<chrome::mojom::SingleFileExtractorListener> listener_;

  base::File src_file_;
  base::File dst_file_;

  SingleFileTarReader tar_reader_;
};
}  // namespace

SingleFileTarFileExtractor::SingleFileTarFileExtractor() = default;
SingleFileTarFileExtractor::~SingleFileTarFileExtractor() = default;

void SingleFileTarFileExtractor::Extract(
    base::File src_file,
    base::File dst_file,
    mojo::PendingRemote<chrome::mojom::SingleFileExtractorListener>
        pending_listener,
    SingleFileExtractor::ExtractCallback callback) {
  if (!src_file.IsValid() || !dst_file.IsValid()) {
    std::move(callback).Run(
        chrome::file_util::mojom::ExtractionResult::kGenericError);
    return;
  }
  TarExtractorInner extractor(std::move(pending_listener), std::move(src_file),
                              std::move(dst_file));
  chrome::file_util::mojom::ExtractionResult result = extractor.Extract();

  // Explicitly close the files before calling `callback` to ensure all
  // extracted data is flushed to the file and the file is closed.
  extractor.CloseFiles();
  std::move(callback).Run(result);
}
