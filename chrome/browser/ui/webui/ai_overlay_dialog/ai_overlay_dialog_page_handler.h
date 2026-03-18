// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AiOverlayDialogPageHandler
    : public ai_overlay_dialog::mojom::PageHandler {
 public:
  explicit AiOverlayDialogPageHandler(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver);
  ~AiOverlayDialogPageHandler() override;

  // overlay_dialog::mojom::PageHandler interface
  void GetApiKey(GetApiKeyCallback callback) override;

 private:
  mojo::Receiver<ai_overlay_dialog::mojom::PageHandler> receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
