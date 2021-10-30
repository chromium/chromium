// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_util.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/base64.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
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

void CopyAndMaintainClipboard(
    std::unique_ptr<ui::ClipboardData> data_with_image,
    const std::string& markup_content,
    std::vector<uint8_t> png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  data_with_image->set_markup_data(markup_content);
  data_with_image->SetPngData(std::move(png_data));
  ui::ClipboardNonBacked::GetForCurrentThread()->WriteClipboardData(
      std::move(data_with_image));
}

/*
 * `clipboard_sequence` is the versioning of the clipboard when we start our
 * copy operation.
 * `callback` alerts whether or not the image was copied to the clipboard. If
 * the image is copied it will return true, otherwise if the image is not copied
 * because the `clipboard_sequence` does not match, it will return false.
 * `png_data` and `html` are different formats of the same image which we are
 * attempting to copy to the clipboard.
 */
void CopyImageToClipboard(ui::ClipboardSequenceNumberToken clipboard_sequence,
                          base::OnceCallback<void(bool)> callback,
                          std::string html,
                          std::vector<uint8_t> png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::ClipboardSequenceNumberToken current_sequence =
      ui::ClipboardNonBacked::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste);
  if (current_sequence != clipboard_sequence) {
    // Clipboard data changed and this copy operation is no longer relevant.
    std::move(callback).Run(false);
    return;
  }
  std::unique_ptr<ui::ClipboardData> current_data =
      std::make_unique<ui::ClipboardData>(
          *ui::ClipboardNonBacked::GetForCurrentThread()->GetClipboardData(
              nullptr));

  // Before modifying the clipboard, remove the old entry in ClipboardHistory.
  // CopyAndMaintainClipboard will write to the clipboard a second time,
  // creating a new entry in clipboard history.
  auto* clipboard_history = ash::ClipboardHistoryController::Get();
  if (clipboard_history) {
    clipboard_history->DeleteClipboardItemByClipboardData(current_data.get());
  }
  CopyAndMaintainClipboard(std::move(current_data), html, std::move(png_data));
  std::move(callback).Run(true);
}

/*
 * `clipboard_sequence` is the versioning of the clipboard when we start our
 * copy operation.
 * `callback` alerts whether or not the image was copied to the clipboard. If
 * the image is copied it will return true, otherwise if the image is not copied
 * because the `clipboard_sequence` does not match, it will return false.
 * `decoded_image` and `html` are different formats of the same image which we
 * are attempting to copy to the clipboard. */
void CopyDecodedImageToClipboard(
    ui::ClipboardSequenceNumberToken clipboard_sequence,
    base::OnceCallback<void(bool)> callback,
    std::string html,
    const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteHTML(base::UTF8ToUTF16(html), std::string());
  clipboard_writer.WriteImage(decoded_image);
  std::move(callback).Run(true);
}

}  // namespace

void ReadFileAndCopyToClipboardLocal(const base::FilePath& local_file) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  auto png_data = base::MakeRefCounted<base::RefCountedString>();
  if (!base::ReadFileToString(local_file, &(png_data->data()))) {
    LOG(ERROR) << "Failed to read the screenshot file: " << local_file.value();
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MaybeDecodeImageFileAndCopyToClipboard,
                     /*clipboard_sequence=*/ui::ClipboardSequenceNumberToken(),
                     /*maintain_clipboard=*/false, png_data,
                     base::DoNothing()));
}

void MaybeDecodeImageFileAndCopyToClipboard(
    ui::ClipboardSequenceNumberToken clipboard_sequence,
    bool maintain_clipboard,
    scoped_refptr<base::RefCountedString> png_data,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Send both HTML and and Image formats to clipboard. HTML format is needed
  // by ARC, while Image is needed by Hangout.
  static const char kImageClipboardFormatPrefix[] =
      "<img src='data:image/png;base64,";
  static const char kImageClipboardFormatSuffix[] = "'>";

  std::string encoded =
      base::Base64Encode(base::as_bytes(base::make_span(png_data->data())));
  std::string html = base::StrCat(
      {kImageClipboardFormatPrefix, encoded, kImageClipboardFormatSuffix});

  if (!maintain_clipboard ||
      !ui::ClipboardNonBacked::GetForCurrentThread()->GetClipboardData(
          nullptr)) {
    // Decode the image in sandboxed process because |png_data| comes from
    // external storage.
    data_decoder::DecodeImageIsolated(
        base::as_bytes(base::make_span(png_data->data())),
        data_decoder::mojom::ImageCodec::kDefault,
        /*shrink_to_fit=*/false, data_decoder::kDefaultMaxSizeInBytes,
        gfx::Size(),
        base::BindOnce(&CopyDecodedImageToClipboard, clipboard_sequence,
                       std::move(callback), std::move(html)));
  } else {
    // Convert string PNG data to a vector of bytes.
    std::vector<uint8_t> png_bytes(png_data->data().begin(),
                                   png_data->data().end());
    // Skip image decoding and write the PNG bytes directly to the clipboard.
    CopyImageToClipboard(clipboard_sequence, std::move(callback),
                         std::move(html), std::move(png_bytes));
  }
}
}  // namespace clipboard_util
