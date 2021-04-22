// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_HANDLER_H_

#include "base/callback.h"

#include "chrome/browser/ui/webui/download_shelf/download_shelf.mojom.h"

class DownloadShelfHandler {
 public:
  virtual ~DownloadShelfHandler() = default;

  virtual void GetDownloads(
      download_shelf::mojom::PageHandler::GetDownloadsCallback callback) = 0;

  virtual void ShowContextMenu(uint32_t download_id,
                               int32_t client_x,
                               int32_t client_y) = 0;

  // Notify the view to show a new download.
  virtual void DoShowDownload(DownloadUIModel* download_model) = 0;

  virtual void OnDownloadUpdated(DownloadUIModel* download_model) = 0;

  virtual void OnDownloadErased(uint32_t download_id) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_HANDLER_H_
