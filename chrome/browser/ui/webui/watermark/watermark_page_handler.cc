// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/watermark/watermark_page_handler.h"

#include "base/types/to_address.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "chrome/browser/enterprise/watermark/watermark_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"

WatermarkPageHandler::WatermarkPageHandler(
    mojo::PendingReceiver<watermark::mojom::PageHandler> receiver,
    content::WebContents& host_contents)
    : host_contents_(host_contents), receiver_(this, std::move(receiver)) {}

WatermarkPageHandler::~WatermarkPageHandler() = default;

void WatermarkPageHandler::SetWatermarkStyle(
    watermark::mojom::WatermarkStylePtr style) {
  auto* bwi =
      webui::GetBrowserWindowInterface(base::to_address(host_contents_));
  // The Watermark WebUI loads only in browser-associated contexts.
  CHECK(bwi);

  // TODO(crbug.com/428946261): Update WatermarkView to use UnownedUserData and
  // fetch it directly from the BrowserWindowInterface.
  enterprise_watermark::WatermarkView* watermark_view =
      bwi->GetBrowserForMigrationOnly()->GetBrowserView().watermark_view();
  if (!watermark_view) {
    return;
  }

  watermark_view->SetString(
      "Watermark Test Page",
      SkColorSetA(
          enterprise_watermark::kBaseFillRGB,
          enterprise_watermark::PercentageToSkAlpha(style->fill_opacity)),
      SkColorSetA(
          enterprise_watermark::kBaseOutlineRGB,
          enterprise_watermark::PercentageToSkAlpha(style->outline_opacity)),
      style->font_size);
}
