// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_SYNCED_SCREENSHOT_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_SYNCED_SCREENSHOT_DATA_SOURCE_H_

#include <string>

#include "content/public/browser/url_data_source.h"

// URLDataSource to serve synced screenshot data.
// URL format: chrome://synced-screenshot/<session_tag>/<tab_id>
class SyncedScreenshotDataSource : public content::URLDataSource {
 public:
  SyncedScreenshotDataSource();

  SyncedScreenshotDataSource(const SyncedScreenshotDataSource&) = delete;
  SyncedScreenshotDataSource& operator=(const SyncedScreenshotDataSource&) =
      delete;

  ~SyncedScreenshotDataSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool ShouldReplaceExistingSource() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_SYNCED_SCREENSHOT_DATA_SOURCE_H_
