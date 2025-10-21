// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_PAGE_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/watermark/watermark.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}

class WatermarkPageHandler : public watermark::mojom::PageHandler {
 public:
  WatermarkPageHandler(
      mojo::PendingReceiver<watermark::mojom::PageHandler> receiver,
      content::WebContents& host_contents);

  ~WatermarkPageHandler() override;

  void SetWatermarkStyle(watermark::mojom::WatermarkStylePtr style) override;

  void ShowNotificationToast() override;

 private:
  const raw_ref<content::WebContents> host_contents_;
  mojo::Receiver<watermark::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_PAGE_HANDLER_H_
