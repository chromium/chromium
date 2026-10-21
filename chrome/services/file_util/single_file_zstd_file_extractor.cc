// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/single_file_zstd_file_extractor.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace {

// Custom deleter for libzstd's streaming decompression context (`ZSTD_DCtx`).
struct ZstdDecompressionContextDeleter {
  void operator()(ZSTD_DCtx* dctx) const { ZSTD_freeDCtx(dctx); }
};

using ScopedZstdDecompressionContext =
    std::unique_ptr<ZSTD_DCtx, ZstdDecompressionContextDeleter>;

chrome::file_util::mojom::ExtractionResult ExtractZstdStream(
    base::File& src_file,
    base::File& dst_file,
    const mojo::Remote<chrome::mojom::SingleFileExtractorListener>& listener) {
  const int64_t total_bytes = src_file.GetLength();
  if (total_bytes <= 0) {
    return chrome::file_util::mojom::ExtractionResult::kGenericError;
  }

  ScopedZstdDecompressionContext dctx(ZSTD_createDCtx());
  if (!dctx || ZSTD_isError(ZSTD_initDStream(dctx.get()))) {
    return chrome::file_util::mojom::ExtractionResult::kGenericError;
  }

  std::vector<uint8_t> in_buffer(ZSTD_DStreamInSize());
  std::vector<uint8_t> out_buffer(ZSTD_DStreamOutSize());

  uint64_t processed_bytes = 0;
  size_t last_ret = 1;
  bool decoded_any_input = false;

  while (true) {
    const std::optional<size_t> bytes_read =
        src_file.ReadAtCurrentPos(in_buffer);
    if (!bytes_read.has_value()) {
      return chrome::file_util::mojom::ExtractionResult::kGenericError;
    }
    if (*bytes_read == 0) {
      break;
    }

    decoded_any_input = true;
    processed_bytes += *bytes_read;

    ZSTD_inBuffer input = {in_buffer.data(), *bytes_read, 0};
    bool wrote_output = false;
    while (input.pos < input.size) {
      ZSTD_outBuffer output = {out_buffer.data(), out_buffer.size(), 0};
      const size_t ret = ZSTD_decompressStream(dctx.get(), &output, &input);
      if (ZSTD_isError(ret)) {
        return chrome::file_util::mojom::ExtractionResult::kGenericError;
      }
      last_ret = ret;

      if (output.pos > 0) {
        if (!dst_file.WriteAtCurrentPosAndCheck(
                base::span(out_buffer).first(output.pos))) {
          return chrome::file_util::mojom::ExtractionResult::kDstFileError;
        }
        wrote_output = true;
      }
    }

    if (wrote_output || last_ret == 0) {
      listener->OnProgress(static_cast<uint64_t>(total_bytes), processed_bytes);
    }
  }

  if (!decoded_any_input || last_ret != 0) {
    return chrome::file_util::mojom::ExtractionResult::kGenericError;
  }

  return chrome::file_util::mojom::ExtractionResult::kSuccess;
}

}  // namespace

SingleFileZstdFileExtractor::SingleFileZstdFileExtractor() = default;

SingleFileZstdFileExtractor::~SingleFileZstdFileExtractor() = default;

void SingleFileZstdFileExtractor::Extract(
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

  mojo::Remote<chrome::mojom::SingleFileExtractorListener> listener(
      std::move(pending_listener));
  chrome::file_util::mojom::ExtractionResult result =
      ExtractZstdStream(src_file, dst_file, listener);

  // Explicitly close the files before calling `callback` to ensure all
  // extracted data is flushed to the destination file and closed.
  src_file.Close();
  dst_file.Close();
  std::move(callback).Run(result);
}
