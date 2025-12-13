// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/watermark/watermark_page_handler.h"

#include "base/types/to_address.h"
#include "chrome/browser/enterprise/data_protection/data_protection_ui_controller.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
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

  auto* data_protection_ui_controller =
      enterprise_data_protection::DataProtectionUIController::From(bwi);
  CHECK(data_protection_ui_controller);

  data_protection_ui_controller->ApplyWatermarkSettings(
      "Watermark Test Page",
      SkColorSetA(
          enterprise_watermark::kBaseFillRGB,
          enterprise_watermark::PercentageToSkAlpha(style->fill_opacity)),
      SkColorSetA(
          enterprise_watermark::kBaseOutlineRGB,
          enterprise_watermark::PercentageToSkAlpha(style->outline_opacity)),
      style->font_size);
}

void WatermarkPageHandler::ShowNotificationToast() {
  auto* bwi =
      webui::GetBrowserWindowInterface(base::to_address(host_contents_));
  if (!bwi) {
    return;
  }

  BrowserWindowFeatures& features = bwi->GetFeatures();
  ToastController* const toast_controller = features.toast_controller();
  if (toast_controller) {
    ToastParams params(ToastId::kCopiedToClipboard);
    toast_controller->MaybeShowToast(std::move(params));
  }
}
