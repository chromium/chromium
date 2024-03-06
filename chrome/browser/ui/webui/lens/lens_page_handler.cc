// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_page_handler.h"

namespace lens {

LensPageHandler::LensPageHandler(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

LensPageHandler::~LensPageHandler() = default;

}  // namespace lens
