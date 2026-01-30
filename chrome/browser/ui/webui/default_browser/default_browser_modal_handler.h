// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebUI;
}

namespace default_browser {
class DefaultBrowserController;
}

// The handler for Javascript messages related to the "default-browser-modal"
// page.
class DefaultBrowserModalHandler final
    : public default_browser_modal::mojom::PageHandler {
 public:
  explicit DefaultBrowserModalHandler(
      content::WebUI* web_ui,
      mojo::PendingReceiver<default_browser_modal::mojom::PageHandler>
          receiver);

  DefaultBrowserModalHandler(const DefaultBrowserModalHandler&) = delete;
  DefaultBrowserModalHandler& operator=(const DefaultBrowserModalHandler&) =
      delete;

  ~DefaultBrowserModalHandler() override;

  // default_browser_modal::mojom::PageHandler:
  void Cancel() override;
  void Confirm() override;

 private:
  raw_ptr<content::WebUI> web_ui_;
  std::unique_ptr<default_browser::DefaultBrowserController> controller_;
  mojo::Receiver<default_browser_modal::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_HANDLER_H_
