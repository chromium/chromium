// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_EMBEDDER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_EMBEDDER_H_

#include "ui/gfx/geometry/point.h"

class DownloadUIModel;

class DownloadShelfUIEmbedder {
 public:
  DownloadShelfUIEmbedder() = default;
  virtual ~DownloadShelfUIEmbedder() = default;

  virtual void DoClose() = 0;

  virtual void DoShowAll() = 0;

  // Show the context menu for |download| at |position| in container's
  // coordinate.
  virtual void ShowDownloadContextMenu(
      DownloadUIModel* download,
      const gfx::Point& position,
      base::OnceClosure on_menu_will_show_callback) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_SHELF_DOWNLOAD_SHELF_UI_EMBEDDER_H_
