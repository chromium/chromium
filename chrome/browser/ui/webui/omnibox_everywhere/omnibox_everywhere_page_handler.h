// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/mojom/omnibox_everywhere.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OmniboxEverywhereUI;

namespace gfx {
class Rect;
}

class OmniboxEverywherePageHandler
    : public omnibox_everywhere::mojom::PageHandler {
 public:
  OmniboxEverywherePageHandler(
      mojo::PendingReceiver<omnibox_everywhere::mojom::PageHandler> receiver,
      mojo::PendingRemote<omnibox_everywhere::mojom::Page> page,
      OmniboxEverywhereUI* web_ui_controller);

  OmniboxEverywherePageHandler(const OmniboxEverywherePageHandler&) = delete;
  OmniboxEverywherePageHandler& operator=(const OmniboxEverywherePageHandler&) =
      delete;

  ~OmniboxEverywherePageHandler() override;

  // omnibox_everywhere::mojom::PageHandler:
  void ShowContextActionMenu(const gfx::Rect& anchor_rect) override;

  // Forwards context menu / composebox events to the WebUI page.
  void OnContextMenuClosed();
  void OpenComposebox(
      omnibox_everywhere::mojom::ComposeboxInitialStatePtr initial_state);

 private:
  mojo::Receiver<omnibox_everywhere::mojom::PageHandler> receiver_;
  mojo::Remote<omnibox_everywhere::mojom::Page> page_;
  raw_ptr<OmniboxEverywhereUI> web_ui_controller_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_PAGE_HANDLER_H_
