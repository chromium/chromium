// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/glic/glic_page_handler.h"

#include "base/version_info/version_info.h"

namespace glic {

GlicPageHandler::GlicPageHandler(
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<glic::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

GlicPageHandler::~GlicPageHandler() = default;

void GlicPageHandler::GetChromeVersion(GetChromeVersionCallback callback) {
  std::move(callback).Run(version_info::GetVersion());
}

}  // namespace glic
