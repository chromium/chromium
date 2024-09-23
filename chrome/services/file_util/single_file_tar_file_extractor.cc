// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/file_util/single_file_tar_file_extractor.h"

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/single_file_tar_reader.h"

namespace {

constexpr int kTarBufferSize = 8192;

// TarExtractorInner extracts a .TAR file and writes the extracted data to the
// destination file.
class TarExtractorInner {
 public:
  TarExtractorInner(
      mojo::PendingRemote<chrome::mojom::SingleFileExtractorListener>
          pending_listener,
      base::File src_file,
      base::File dst_file)
      : listener_(std::move(pending_listener)),
        src_file_(std::move(src_file)),
        dst_file_(std::move(dst_file)) {}

  chrome::file_util::mojom::ExtractionResult Extract() {
    std::vector<uint8_t> tar_buffer;
    while (true) {
      tar_buffer = ReadTarFile();
      if (tar_buffer.empty()) {
        return chrome::file_util::mojom::ExtractionResult::kGenericError;
      }

      base::span<const uint8_t> output_file_content;
      if (!tar_reader_.ExtractChunk(base::make_span(tar_buffer),
                                    output_file_content)) {
        return chrome::file_util::mojom::ExtractionResult::kInvalidSrcFile;
      }

      // TODO(sahok): Utilize this logic and Deduplicate the same logic in
      // single_file_tar_xz_file_extractor.cc
      if (!dst_file_.WriteAtCurrentPosAndCheck(output_file_content))
        return chrome::file_util::mojom::ExtractionResult::kDstFileError;

      if (tar_reader_.IsComplete())
        return chrome::file_util::mojom::ExtractionResult::kSuccess;

      listener_->OnProgress(tar_reader_.tar_content_size().value(),
                            tar_reader_.bytes_processed());
    }
  }

  void CloseFiles() {
    src_file_.Close();
    dst_file_.Close();
  }

 private:
  // ReadTarFile reads .tar file and returns the contents. ReadTarFile reads
  // kTarBufferSize bytes at most, and might return an empty vector on error.
  // Returned vector is resized to actual bytes read.
  std::vector<uint8_t> ReadTarFile() {
    std::vector<uint8_t> tar_buffer(kTarBufferSize);
    const int bytes_read = src_file_.ReadAtCurrentPos(
        reinterpret_cast<char*>(tar_buffer.data()), kTarBufferSize);

    if (bytes_read < 0)
      return std::vector<uint8_t>();
    if (bytes_read == 0) {
      // After reading the last chunk of file content, it is expected that
      // tar_reader_.IsComplete() in the Extract function returns true and the
      // .tar.xz file extraction ends.
      return std::vector<uint8_t>();
    }
    tar_buffer.resize(bytes_read);
    return tar_buffer;
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
