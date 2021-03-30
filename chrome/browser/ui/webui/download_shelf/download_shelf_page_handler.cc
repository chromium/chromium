// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_shelf/download_shelf_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/download_shelf/download_shelf_ui.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace download {
class DownloadItem;
}

DownloadShelfPageHandler::DownloadShelfPageHandler(
    mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver,
    mojo::PendingRemote<download_shelf::mojom::Page> page,
    content::WebUI* web_ui,
    DownloadShelfUI* download_shelf_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      download_shelf_ui_(download_shelf_ui) {}

DownloadShelfPageHandler::~DownloadShelfPageHandler() = default;

void DownloadShelfPageHandler::ShowContextMenu(uint32_t download_id,
                                               int32_t client_x,
                                               int32_t client_y) {
  download_shelf_ui_->ShowContextMenu(download_id, client_x, client_y);
}
