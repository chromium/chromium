// Copyright 2014 The Chromium Authors. All rights reserved.
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

#if defined(TOOLKIT_VIEWS)

// static
const ui::ClipboardFormatType& BookmarkNodeData::GetBookmarkFormatType() {
  static const base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::GetType(
          base::SysNSStringToUTF8(kUTTypeChromiumBookmarkDictionaryList)));

  return *format;
}

#endif  // TOOLKIT_VIEWS

// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  NSPasteboard* pb =
      ui::ClipboardUtil::PasteboardFromBuffer(ui::ClipboardBuffer::kCopyPaste);
  return PasteboardContainsBookmarks(pb);
}

void BookmarkNodeData::WriteToClipboard() {
  NSPasteboard* pb =
      ui::ClipboardUtil::PasteboardFromBuffer(ui::ClipboardBuffer::kCopyPaste);
  WriteBookmarksToPasteboard(pb, elements, profile_path_);
}

bool BookmarkNodeData::ReadFromClipboard(ui::ClipboardBuffer buffer) {
  NSPasteboard* pb = ui::ClipboardUtil::PasteboardFromBuffer(buffer);
  base::FilePath file_path;
  if (ReadBookmarksFromPasteboard(pb, &elements, &file_path)) {
    profile_path_ = file_path;
    return true;
  }

  return false;
}

#if defined(TOOLKIT_VIEWS)

void BookmarkNodeData::Write(const base::FilePath& profile_path,
                             ui::OSExchangeData* data) const {
  ui::OSExchangeDataProviderMac& provider =
      static_cast<ui::OSExchangeDataProviderMac&>(data->provider());
  NSPasteboard* pb = provider.GetPasteboard();
  WriteBookmarksToPasteboard(pb, elements, profile_path);
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
