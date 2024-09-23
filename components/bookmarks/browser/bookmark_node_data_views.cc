// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/url_constants.h"

namespace bookmarks {

// static
const ui::ClipboardFormatType& BookmarkNodeData::GetBookmarkFormatType() {
  static const base::NoDestructor<ui::ClipboardFormatType> format(
      ui::ClipboardFormatType::GetType(
          BookmarkNodeData::kClipboardFormatString));

  return *format;
}

void BookmarkNodeData::Write(const base::FilePath& profile_path,
                             ui::OSExchangeData* data) const {
  DCHECK(data);

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (has_single_url()) {
    if (elements[0].url.SchemeIs(url::kJavaScriptScheme)) {
      data->SetString(base::UTF8ToUTF16(elements[0].url.spec()));
    } else {
      data->SetURL(elements[0].url, elements[0].title);
    }
  }

  base::Pickle data_pickle;
  WriteToPickle(profile_path, &data_pickle);

  data->SetPickledData(GetBookmarkFormatType(), data_pickle);
}

bool BookmarkNodeData::Read(const ui::OSExchangeData& data) {
  elements.clear();

  profile_path_.clear();

  if (data.HasCustomFormat(GetBookmarkFormatType())) {
    if (std::optional<base::Pickle> drag_data_pickle =
            data.GetPickledData(GetBookmarkFormatType());
        drag_data_pickle.has_value()) {
      if (!ReadFromPickle(&drag_data_pickle.value())) {
        return false;
      }
    }
  } else if (std::optional<ui::OSExchangeData::UrlInfo> result =
                 data.GetURLAndTitle(
                     ui::FilenameToURLPolicy::CONVERT_FILENAMES);
             result.has_value()) {
    ReadFromTuple(result->url, result->title);
  }

  return is_valid();
}

}  // namespace bookmarks
