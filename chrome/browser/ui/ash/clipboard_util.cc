// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/clipboard_util.h"

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace clipboard_util {
namespace {

const char kImageClipboardFormatPrefix[] = "<img src='data:image/png;base64,";
const char kImageClipboardFormatSuffix[] = "'>";

void CopyImageToClipboard(scoped_refptr<base::RefCountedString> png_data,
                          const SkBitmap& decoded_image) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string encoded;
  base::Base64Encode(png_data->data(), &encoded);
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);

    // Send both HTML and and Image formats to clipboard. HTML format is needed
    // by ARC, while Image is needed by Hangout.
    std::string html(kImageClipboardFormatPrefix);
    html += encoded;
    html += kImageClipboardFormatSuffix;
    scw.WriteHTML(base::UTF8ToUTF16(html), std::string());
    scw.WriteImage(decoded_image);
  }
}

}  // namespace

void ReadFileAndCopyToClipboardLocal(const base::FilePath& local_file) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  scoped_refptr<base::RefCountedString> png_data(new base::RefCountedString());
  if (!base::ReadFileToString(local_file, &(png_data->data()))) {
    LOG(ERROR) << "Failed to read the screenshot file: " << local_file.value();
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DecodeImageFileAndCopyToClipboard, png_data));
}

void DecodeImageFileAndCopyToClipboard(
    scoped_refptr<base::RefCountedString> png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Decode the image in sandboxed process because |png_data| comes from
  // external storage.
  data_decoder::DecodeImageIsolated(
      std::vector<uint8_t>(png_data->data().begin(), png_data->data().end()),
      data_decoder::mojom::ImageCodec::DEFAULT, false,
      data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&CopyImageToClipboard, png_data));
}
}  // namespace clipboard_util
