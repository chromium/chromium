// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SOURCE_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SOURCE_H_

namespace download {

// The source of download.
// Used in UMA metrics and persisted to disk.
// Entries in this enum can only be appended instead of being deleted or reused.
// Any changes here also needs to apply to histogram_suffixes_list.xml.
enum class DownloadSource {
  // The source is unknown.
  UNKNOWN = 0,

  // Download is triggered from navigation request.
  NAVIGATION = 1,

  // Drag and drop.
  DRAG_AND_DROP = 2,

  // Renderer initiated download, mostly from Javascript or HTML <a> tag.
  FROM_RENDERER = 3,

  // Extension download API.
  EXTENSION_API = 4,

  // Extension web store installer.
  EXTENSION_INSTALLER = 5,

  // Download service API background download.
  INTERNAL_API = 6,

  // Download through web contents API.
  WEB_CONTENTS_API = 7,

  // Offline page download.
  OFFLINE_PAGE = 8,

  // Context menu download.
  CONTEXT_MENU = 9,

  // Retry download. On desktop, the download is initiated in chrome://download
  // web UI. On Android, there is no renderer associated with it.
  RETRY = 10,

  // Retry download triggered through the downloads bubble.
  RETRY_FROM_BUBBLE = 11,

  // Toolbar menu, only available on Android.
  TOOLBAR_MENU = 12,
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_SOURCE_H_
