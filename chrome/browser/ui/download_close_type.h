// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_CLOSE_TYPE_H_
#define CHROME_BROWSER_UI_DOWNLOAD_CLOSE_TYPE_H_

// The context for a download blocked notification from
// UnloadController::OkToCloseWithInProgressDownloads.
//
// This lives apart from UnloadController so that callers which only need to
// name the enum - notably browser_window.h - do not have to depend on the
// controller.
enum class DownloadCloseType {
  // Browser close is not blocked by download state.
  kOk,

  // The browser is shutting down and there are active downloads
  // that would be cancelled.
  kBrowserShutdown,

  // There are active downloads associated with this incognito profile
  // that would be canceled.
  kLastWindowInIncognitoProfile,

  // There are active downloads associated with this guest session
  // that would be canceled.
  kLastWindowInGuestSession,
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_CLOSE_TYPE_H_
