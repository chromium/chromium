// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_shelf/download_shelf_page_handler.h"

DownloadShelfPageHandler::DownloadShelfPageHandler(
    mojo::PendingReceiver<download_shelf::mojom::PageHandler> receiver,
    mojo::PendingRemote<download_shelf::mojom::Page> page,
    content::WebUI* web_ui,
    ui::MojoWebUIController* webui_controller)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

DownloadShelfPageHandler::~DownloadShelfPageHandler() = default;
