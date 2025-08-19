// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/clipboard/clipboard_util.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/geometry/size.h"

namespace clipboard_util {
namespace {
std::optional<std::string> ReadFileContent(const base::FilePath& local_file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  std::string png_data;
  if (!base::ReadFileToString(local_file, &png_data)) {
    return std::nullopt;
  }
  return png_data;
}

/*
 * `decoded_image` and `html` are different formats of the same image which we
 * are attempting to copy to the clipboard. */
void CopyDecodedImageToClipboard(ReadFileAndCopyToClipboardCallback callback,
                                 std::string html,
                                 const SkBitmap& decoded_image) {
  if (decoded_image.isNull()) {
    std::move(callback).Run(
        ReadFileAndCopyToClipboardResult::kFailedToDecodeImage);
    return;
  }

  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteHTML(base::UTF8ToUTF16(html), std::string());
  clipboard_writer.WriteImage(decoded_image);

  std::move(callback).Run(ReadFileAndCopyToClipboardResult::kSuccess);
}

void DecodeImageAndCopyToClipboard(ReadFileAndCopyToClipboardCallback callback,
                                   std::optional<std::string> png_data) {
  if (!png_data) {
    std::move(callback).Run(
        ReadFileAndCopyToClipboardResult::kFailedToReadFile);
    return;
  }

  // Send both HTML and and Image formats to clipboard. HTML format is needed
  // by ARC, while Image is needed by Hangout.
  static const char kImageClipboardFormatPrefix[] =
      "<img src='data:image/png;base64,";
  static const char kImageClipboardFormatSuffix[] = "'>";

  std::string encoded =
      base::Base64Encode(base::as_byte_span(png_data.value()));
  std::string html = base::StrCat(
      {kImageClipboardFormatPrefix, encoded, kImageClipboardFormatSuffix});

  // Decode the image in sandboxed process because |png_data| comes from
  // external storage.
  data_decoder::DecodeImageIsolated(
      base::as_byte_span(png_data.value()),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      gfx::Size(),
      base::BindOnce(&CopyDecodedImageToClipboard, std::move(callback),
                     std::move(html)));
}

}  // namespace

void ReadFileAndCopyToClipboard(const base::FilePath& local_file,
                                ReadFileAndCopyToClipboardCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadFileContent, local_file),
      base::BindOnce(&DecodeImageAndCopyToClipboard, std::move(callback)));
}

}  // namespace clipboard_util
