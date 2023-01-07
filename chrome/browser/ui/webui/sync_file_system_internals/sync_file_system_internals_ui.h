// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

class SyncFileSystemInternalsUI : public content::WebUIController {
 public:
  explicit SyncFileSystemInternalsUI(content::WebUI* web_ui);

  SyncFileSystemInternalsUI(const SyncFileSystemInternalsUI&) = delete;
  SyncFileSystemInternalsUI& operator=(const SyncFileSystemInternalsUI&) =
      delete;

  ~SyncFileSystemInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_SYNC_FILE_SYSTEM_INTERNALS_UI_H_
