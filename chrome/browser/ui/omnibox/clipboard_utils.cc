// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/clipboard_utils.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/omnibox/browser/omnibox_text_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace {

void OnGetAvailableFormats(GetClipboardTextCallback callback,
                           bool notify_if_restricted,
                           base::flat_set<ui::ClipboardFormatType> formats) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst =
      ui::DataTransferEndpoint(ui::EndpointType::kDefault,
                               {.notify_if_restricted = notify_if_restricted});

  // Try text format.
  if (formats.contains(ui::ClipboardFormatType::PlainTextType())) {
    clipboard->ReadText(
        ui::ClipboardBuffer::kCopyPaste, data_dst,
        base::BindOnce(
            [](GetClipboardTextCallback callback, std::u16string text) {
              text = text.substr(0, kMaxClipboardTextLength);
              std::move(callback).Run(omnibox::SanitizeTextForPaste(text));
            },
            std::move(callback)));
    return;
  }

  // Try bookmark format.
  //
  // It is tempting to try bookmark format first, but the URL we get out of a
  // bookmark has been cannonicalized via GURL.  This means if a user copies
  // and pastes from the URL bar to itself, the text will get fixed up and
  // cannonicalized, which is not what the user expects.  By pasting in this
  // order, we are sure to paste what the user copied.
  if (formats.contains(ui::ClipboardFormatType::UrlType())) {
    clipboard->ReadBookmark(
        data_dst,
        base::BindOnce(
            [](GetClipboardTextCallback callback, std::u16string title,
               GURL url) {
              if (url.is_valid()) {
                std::move(callback).Run(omnibox::StripJavascriptSchemas(
                    base::UTF8ToUTF16(url.spec())));
              } else {
                std::move(callback).Run(std::u16string());
              }
            },
            std::move(callback)));
    return;
  }

  std::move(callback).Run(std::u16string());
}

}  // namespace

void GetClipboardText(bool notify_if_restricted,
                      GetClipboardTextCallback callback) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  ui::DataTransferEndpoint data_dst =
      ui::DataTransferEndpoint(ui::EndpointType::kDefault,
                               {.notify_if_restricted = notify_if_restricted});
  clipboard->GetAllAvailableFormats(
      ui::ClipboardBuffer::kCopyPaste, data_dst,
      base::BindOnce(&OnGetAvailableFormats, std::move(callback),
                     notify_if_restricted));
}
