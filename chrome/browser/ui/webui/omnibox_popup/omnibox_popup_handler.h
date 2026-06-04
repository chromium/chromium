// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}

class OmniboxPopupHandler : public omnibox_popup::mojom::PageHandler {
 public:
  OmniboxPopupHandler(
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver,
      mojo::PendingRemote<omnibox_popup::mojom::Page> page,
      content::WebContents* web_contents);

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

  // omnibox_popup::mojom::Page:
  void OnShow();
  void OnContextMenuClosed();
  void SetInputText(const std::string& text);

 private:
  mojo::Receiver<omnibox_popup::mojom::PageHandler> receiver_;
  mojo::Remote<omnibox_popup::mojom::Page> page_;
  base::WeakPtr<TopChromeWebUIController::Embedder> embedder_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_
