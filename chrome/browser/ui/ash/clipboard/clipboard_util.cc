// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard/clipboard_util.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace clipboard_util {
namespace {

/*
 * `clipboard_sequence` is the versioning of the clipboard when we start our
 * copy operation.
 * `decoded_image` and `html` are different formats of the same image which we
 * are attempting to copy to the clipboard. */
void CopyDecodedImageToClipboard(
    ui::ClipboardSequenceNumberToken clipboard_sequence,
    std::string html,
    const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteHTML(base::UTF8ToUTF16(html), std::string());
  clipboard_writer.WriteImage(decoded_image);
}

}  // namespace

void ReadFileAndCopyToClipboardLocal(const base::FilePath& local_file) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  std::string png_data;
  if (!base::ReadFileToString(local_file, &png_data)) {
    LOG(ERROR) << "Failed to read the screenshot file: " << local_file.value();
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DecodeImageFileAndCopyToClipboard,
                     /*clipboard_sequence=*/ui::ClipboardSequenceNumberToken(),
                     std::move(png_data)));
}

void DecodeImageFileAndCopyToClipboard(
    ui::ClipboardSequenceNumberToken clipboard_sequence,
    std::string png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Send both HTML and and Image formats to clipboard. HTML format is needed
  // by ARC, while Image is needed by Hangout.
  static const char kImageClipboardFormatPrefix[] =
      "<img src='data:image/png;base64,";
  static const char kImageClipboardFormatSuffix[] = "'>";

  std::string encoded =
      base::Base64Encode(base::as_bytes(base::make_span(png_data)));
  std::string html = base::StrCat(
      {kImageClipboardFormatPrefix, encoded, kImageClipboardFormatSuffix});

  // Decode the image in sandboxed process because |png_data| comes from
  // external storage.
  data_decoder::DecodeImageIsolated(
      base::as_bytes(base::make_span(png_data)),
      data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
      gfx::Size(),
      base::BindOnce(&CopyDecodedImageToClipboard, clipboard_sequence,
                     std::move(html)));
}
}  // namespace clipboard_util
