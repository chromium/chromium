// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}

class OmniboxController;

class OmniboxPopupHandler : public omnibox_popup::mojom::PageHandler {
 public:
  OmniboxPopupHandler(
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver,
      mojo::PendingRemote<omnibox_popup::mojom::Page> page,
      content::WebContents* web_contents,
      OmniboxController* controller);

  OmniboxPopupHandler(const OmniboxPopupHandler&) = delete;
  OmniboxPopupHandler& operator=(const OmniboxPopupHandler&) = delete;

  ~OmniboxPopupHandler() override;

  void set_embedder(
      base::WeakPtr<TopChromeWebUIController::Embedder> embedder) {
    embedder_ = embedder;
  }

  // omnibox_popup::mojom::PageHandler:
  void ShowContextMenu(const gfx::Point& point) override;
  void CloseUI() override;
  void OnManualBlur(uint32_t sequence_number) override;
  void OnSelectionChanged(const gfx::Range& selection,
                          uint32_t sequence_number,
                          bool show_full_url) override;
  void Revert(uint32_t sequence_number) override;
  void LogEscapeAction(
      omnibox_popup::mojom::OmniboxEscapeAction action) override;

  // omnibox_popup::mojom::Page:
  void OnShow();
  void OnContextMenuClosed();
  void SetInputState(const std::string& text,
                     const gfx::Range& selection,
                     bool user_input_in_progress,
                     const std::string& full_url,
                     bool is_focused,
                     const std::string& permanent_display_text,
                     bool show_full_url);

  const gfx::Range& latest_selection() const { return latest_selection_; }
  bool show_full_url() const { return show_full_url_; }

 private:
  mojo::Receiver<omnibox_popup::mojom::PageHandler> receiver_;
  mojo::Remote<omnibox_popup::mojom::Page> page_;
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<OmniboxController> controller_;
  // Caches the latest selection range reported by the WebUI to allow
  // synchronous access on tab switches.
  gfx::Range latest_selection_;
  bool show_full_url_ = false;
  // Monotonically increasing sequence number sent to the WebUI to reject stale
  // selection reports that arrive asynchronously.
  uint32_t current_sequence_number_ = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_
