// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/watermark/watermark_page_handler.h"

WatermarkPageHandler::WatermarkPageHandler(
    mojo::PendingReceiver<watermark::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

WatermarkPageHandler::~WatermarkPageHandler() = default;

void WatermarkPageHandler::SetWatermarkStyle(
    watermark::mojom::WatermarkStylePtr style) {
  // TODO(crbug.com/433606367): Connect Controls to Watermark
  // using style->fill_opacity, style->outline_opacity and style->font_size
}
