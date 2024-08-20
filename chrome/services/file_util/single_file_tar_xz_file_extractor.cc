// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/file_util/single_file_tar_xz_file_extractor.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/single_file_tar_reader.h"
#include "third_party/lzma_sdk/C/7zCrc.h"
#include "third_party/lzma_sdk/C/Xz.h"
#include "third_party/lzma_sdk/C/XzCrc64.h"

namespace {

constexpr int kDefaultBufferSize = 8192;
constexpr int kXzBufferSize = kDefaultBufferSize;
constexpr int kTarBufferSize = kDefaultBufferSize;

// ExtractorInner extracts a .TAR.XZ file and writes the extracted data to the
// output file.
class ExtractorInner {
 public:
  ExtractorInner(mojo::PendingRemote<chrome::mojom::SingleFileExtractorListener>
                     pending_listener,
                 base::File src_file,
                 base::File dst_file)
      : listener_(std::move(pending_listener)),
        src_file_(std::move(src_file)),
        dst_file_(std::move(dst_file)) {
    alloc_.Alloc = [](ISzAllocPtr, size_t size) { return malloc(size); };
    alloc_.Free = [](ISzAllocPtr, void* ptr) { return free(ptr); };
    XzUnpacker_Construct(&state_, &alloc_);
  }

  chrome::file_util::mojom::ExtractionResult Extract() {
    std::vector<uint8_t> xz_buffer(kXzBufferSize);
    while (true) {
      const int bytes_read = src_file_.ReadAtCurrentPos(
          reinterpret_cast<char*>(xz_buffer.data()), xz_buffer.size());
      if (bytes_read < 0)
        return chrome::file_util::mojom::ExtractionResult::kGenericError;
      if (bytes_read == 0) {
        // After reading the last chunk of file content, it is expected that the
        // ExtractChunk() below populates `result` with kSuccess and the .tar.xz
        // file extraction ends.
        return chrome::file_util::mojom::ExtractionResult::kGenericError;
      }

      std::optional<chrome::file_util::mojom::ExtractionResult> result;
      ExtractChunk(
          base::span(xz_buffer).first(base::checked_cast<size_t>(bytes_read)),
          &result);
      if (result.has_value())
        return result.value();

      // TODO(sahok): This value can be 100% when ExtractChunk didn't return
      // kSuccess because only footer is left. This can be confusing and not
      // consistent with single_file_tar_file_extractor. Reorganize and make it
      // clear if OnProgress will return 100% or not.
      listener_->OnProgress(tar_reader_.tar_content_size().value(),
                            tar_reader_.bytes_processed());
    }
  }

  void CloseFiles() {
    src_file_.Close();
    dst_file_.Close();
  }

  ~ExtractorInner() { XzUnpacker_Free(&state_); }

 private:
  void ExtractChunk(
      base::span<const uint8_t> xz_buffer,
      std::optional<chrome::file_util::mojom::ExtractionResult>* result) {
    std::vector<uint8_t> tar_buffer(kTarBufferSize);
    // With the size of tar_buffer, XzUnpacker_Code cannot always extract all
    // data in xz_buffer. Repeat extract until it extracts all data in
    // xz_buffer.
    ECoderStatus status = CODER_STATUS_NOT_FINISHED;
    while (status == CODER_STATUS_NOT_FINISHED) {
      size_t decompressed_size = tar_buffer.size();
      size_t compressed_size = xz_buffer.size();
      int xz_result = XzUnpacker_Code(
          &state_, tar_buffer.data(), &decompressed_size, xz_buffer.data(),
          &compressed_size,
          /*srcFinished=*/xz_buffer.empty(), CODER_FINISH_ANY, &status);
      if (xz_result != SZ_OK) {
        *result = chrome::file_util::mojom::ExtractionResult::kGenericError;
        return;
      }
      xz_buffer = xz_buffer.subspan(compressed_size);

      base::span<const uint8_t> tar_buffer_span =
          base::span(tar_buffer).first(decompressed_size);
      base::span<const uint8_t> output_file_content;
      if (!tar_reader_.ExtractChunk(tar_buffer_span, output_file_content)) {
        *result = chrome::file_util::mojom::ExtractionResult::kInvalidSrcFile;
        return;
      }

      if (!dst_file_.WriteAtCurrentPosAndCheck(output_file_content)) {
        *result = chrome::file_util::mojom::ExtractionResult::kDstFileError;
        return;
      }
    }
    const bool xz_extraction_finished =
        (status == CODER_STATUS_FINISHED_WITH_MARK ||
         (status == CODER_STATUS_NEEDS_MORE_INPUT &&
          XzUnpacker_IsStreamWasFinished(&state_)));
    if (tar_reader_.IsComplete() && xz_extraction_finished)
      *result = chrome::file_util::mojom::ExtractionResult::kSuccess;
  }

  const mojo::Remote<chrome::mojom::SingleFileExtractorListener> listener_;

  CXzUnpacker state_;
  ISzAlloc alloc_;

  base::File src_file_;
  base::File dst_file_;

  SingleFileTarReader tar_reader_;
};

}  // namespace

SingleFileTarXzFileExtractor::SingleFileTarXzFileExtractor() {
  [[maybe_unused]] static const bool initialized = []() {
    CrcGenerateTable();
    Crc64GenerateTable();
    return true;
  }();
}

SingleFileTarXzFileExtractor::~SingleFileTarXzFileExtractor() = default;

void SingleFileTarXzFileExtractor::Extract(
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
  ExtractorInner extractor(std::move(pending_listener), std::move(src_file),
                           std::move(dst_file));
  chrome::file_util::mojom::ExtractionResult result = extractor.Extract();

  // Explicitly close the files before calling `callback` to ensure all
  // extracted data is flushed to the file and the file is closed.
  extractor.CloseFiles();
  std::move(callback).Run(result);
}
