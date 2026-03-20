// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

class AiOverlayDialogPageHandler
    : public ai_overlay_dialog::mojom::PageHandler {
 public:
  explicit AiOverlayDialogPageHandler(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
      mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote);
  ~AiOverlayDialogPageHandler() override;

  // overlay_dialog::mojom::PageHandler interface
  void GetApiKey(GetApiKeyCallback callback) override;
  void GetMockAudioData(GetMockAudioDataCallback callback) override;

  void InvalidatePageContext();
  void UpdateCurrentPageContext(const GURL& url,
                                const std::u16string& title,
                                const std::string& content);

 private:
  mojo::Receiver<ai_overlay_dialog::mojom::PageHandler> receiver_{this};
  mojo::Remote<ai_overlay_dialog::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
