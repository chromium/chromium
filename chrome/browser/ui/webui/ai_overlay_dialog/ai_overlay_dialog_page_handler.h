// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace actions {
class ActionItem;
}

namespace ttc {

class AiOverlayDialogPageHandler
    : public ai_overlay_dialog::mojom::PageHandler,
      public AiOverlayDialogController::Observer {
 public:
  AiOverlayDialogPageHandler(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
      mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote,
      BrowserWindowInterface* browser);
  ~AiOverlayDialogPageHandler() override;

  // overlay_dialog::mojom::PageHandler interface
  void GetMockAudioData(GetMockAudioDataCallback callback) override;
  void UpdateAudioEnergy(float energy) override;

  void DidChangePage(const GURL& url,
                     const std::optional<std::u16string>& title,
                     const std::optional<std::string>& content);
  void UpdateCurrentPageContext(const std::u16string& title,
                                const std::string& content);

  // AiOverlayDialogController::Observer
  void OnCaptionsVisibleChanged(bool visible) override;
  void OnUsePersonaChanged(bool use_persona) override;

 private:
  mojo::Receiver<ai_overlay_dialog::mojom::PageHandler> receiver_;
  mojo::Remote<ai_overlay_dialog::mojom::Page> page_;
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<actions::ActionItem> overlay_action_item_ = nullptr;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
