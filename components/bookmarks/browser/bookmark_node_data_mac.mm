// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_pasteboard_helper_mac.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"

namespace bookmarks {

// static
void BookmarkNodeData::ClipboardContainsBookmarks(
    base::OnceCallback<void(bool)> callback) {
  NSPasteboard* pb =
      ui::clipboard_util::PasteboardFromBuffer(ui::ClipboardBuffer::kCopyPaste);
  std::move(callback).Run(PasteboardContainsBookmarks(pb));
}

void BookmarkNodeData::WriteToClipboard(bool is_off_the_record) {
  NSPasteboard* pb =
      ui::clipboard_util::PasteboardFromBuffer(ui::ClipboardBuffer::kCopyPaste);
  WriteBookmarksToPasteboard(pb, elements, profile_path_, is_off_the_record);
}

// static
void BookmarkNodeData::ReadFromClipboard(
    ui::ClipboardBuffer buffer,
    base::OnceCallback<void(std::unique_ptr<BookmarkNodeData>)> callback) {
  NSPasteboard* pb = ui::clipboard_util::PasteboardFromBuffer(buffer);
  auto data = std::make_unique<BookmarkNodeData>();
  base::FilePath file_path;
  if (ReadBookmarksFromPasteboard(pb, &data->elements, &file_path)) {
    data->profile_path_ = file_path;
    std::move(callback).Run(std::move(data));
    return;
  }

  std::move(callback).Run(nullptr);
}

#if defined(TOOLKIT_VIEWS)

void BookmarkNodeData::Write(const base::FilePath& profile_path,
                             ui::OSExchangeData* data) const {
  ui::OSExchangeDataProviderMac& provider =
      static_cast<ui::OSExchangeDataProviderMac&>(data->provider());
  NSPasteboard* pb = provider.GetPasteboard();
  // TODO(crbug.com/40945200): Add support for off-the-record bookmarks during
  // drag and drop.
  WriteBookmarksToPasteboard(pb, elements, profile_path,
                             /*is_off_the_record=*/false);
}

bool BookmarkNodeData::Read(const ui::OSExchangeData& data) {
  const ui::OSExchangeDataProviderMac& provider =
      static_cast<const ui::OSExchangeDataProviderMac&>(data.provider());
  NSPasteboard* pb = provider.GetPasteboard();
  base::FilePath file_path;
  if (ReadBookmarksFromPasteboard(pb, &elements, &file_path)) {
    profile_path_ = file_path;
    return true;
  }

  return false;
}

#endif  // TOOLKIT_VIEWS

}  // namespace bookmarks
