// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

constexpr int kXzBufferSize = kDefaultBufferSize;

// kTarBufferSize must be less than or equal to kTarBufferSize in
// single_file_tar_reader.cc.
// TODO(b/255682934): Resolve the above comment.
constexpr int kTarBufferSize = kDefaultBufferSize;

// ExtractorInner extracts a .TAR.XZ file and writes the extracted data to the
// output file.
class ExtractorInner : public SingleFileTarReader::Delegate {
 public:
  ExtractorInner(
      mojo::PendingRemote<chrome::mojom::SingleFileTarXzFileExtractorListener>
          pending_listener,
      base::File src_file,
      base::File dst_file)
      : listener_(std::move(pending_listener)),
        src_file_(std::move(src_file)),
        dst_file_(std::move(dst_file)),
        tar_reader_(this) {
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
        return chrome::file_util::mojom::ExtractionResult::kUnzipGenericError;
      if (bytes_read == 0) {
        // After reading the last chunk of file content, it is expected that the
        // ExtractChunk() below populates `result` with kSuccess and the .tar.xz
        // file extraction ends.
        return chrome::file_util::mojom::ExtractionResult::kUnzipGenericError;
      }

      absl::optional<chrome::file_util::mojom::ExtractionResult> result;
      ExtractChunk(
          base::make_span(xz_buffer).first(static_cast<size_t>(bytes_read)),
          &result);
      if (result.has_value())
        return result.value();

      listener_->OnProgress(tar_reader_.total_bytes().value(),
                            tar_reader_.curr_bytes());
    }
  }

  ~ExtractorInner() override { XzUnpacker_Free(&state_); }

 private:
  void ExtractChunk(
      base::span<const uint8_t> xz_buffer,
      absl::optional<chrome::file_util::mojom::ExtractionResult>* result) {
    // With the size of tar_buffer_, XzUnpacker_Code cannot always extract all
    // data in xz_buffer. Repeat extract until it extracts all data in
    // xz_buffer.
    ECoderStatus status = CODER_STATUS_NOT_FINISHED;
    while (status == CODER_STATUS_NOT_FINISHED) {
      size_t decompressed_size = kTarBufferSize;
      size_t compressed_size = xz_buffer.size();
      int xz_result = XzUnpacker_Code(&state_, tar_buffer_, &decompressed_size,
                                      xz_buffer.data(), &compressed_size,
                                      /*srcFinished=*/xz_buffer.empty(),
                                      CODER_FINISH_ANY, &status);
      if (xz_result != SZ_OK) {
        *result =
            chrome::file_util::mojom::ExtractionResult::kUnzipGenericError;
        return;
      }
      xz_buffer = xz_buffer.subspan(compressed_size);

      tar_buffer_size_ = decompressed_size;
      // tar_reader_.ExtractChunk func needs to be called only once, because
      // kTarBufferSize in single_file_tar_reader.cc and kTarBufferSize in this
      // file are the same value.
      // TODO(b/255682934): Resolve the above comment.
      if (tar_reader_.ExtractChunk() != SingleFileTarReader::Result::kSuccess) {
        *result = tar_reader_.error();
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

  // SingleFileTarReader::Delegate implementation.
  SingleFileTarReader::Result ReadTarFile(
      char* data,
      uint32_t* size,
      chrome::file_util::mojom::ExtractionResult* error) override {
    *size = std::min(*size, base::checked_cast<uint32_t>(tar_buffer_size_));
    std::copy(tar_buffer_, tar_buffer_ + *size, data);
    tar_buffer_size_ -= *size;
    return SingleFileTarReader::Result::kSuccess;
  }

  // SingleFileTarReader::Delegate implementation.
  bool WriteContents(
      const char* data,
      int size,
      chrome::file_util::mojom::ExtractionResult* error) override {
    const int bytes_written = dst_file_.WriteAtCurrentPos(data, size);
    if (bytes_written < 0 || bytes_written != size) {
      *error = chrome::file_util::mojom::ExtractionResult::kTempFileError;
      return false;
    }
    return true;
  }

  const mojo::Remote<chrome::mojom::SingleFileTarXzFileExtractorListener>
      listener_;

  CXzUnpacker state_;
  ISzAlloc alloc_;

  base::File src_file_;
  base::File dst_file_;

  SingleFileTarReader tar_reader_;
  uint8_t tar_buffer_[kTarBufferSize];
  size_t tar_buffer_size_ = 0;
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
    mojo::PendingRemote<chrome::mojom::SingleFileTarXzFileExtractorListener>
        pending_listener,
    SingleFileTarXzFileExtractor::ExtractCallback callback) {
  if (!src_file.IsValid() || !dst_file.IsValid()) {
    std::move(callback).Run(
        chrome::file_util::mojom::ExtractionResult::kUnzipGenericError);
    return;
  }
  ExtractorInner* extractor = new ExtractorInner(
      std::move(pending_listener), std::move(src_file), std::move(dst_file));
  chrome::file_util::mojom::ExtractionResult result = extractor->Extract();
  // Destroy the File objects before calling `callback`.
  delete extractor;
  std::move(callback).Run(result);
}
