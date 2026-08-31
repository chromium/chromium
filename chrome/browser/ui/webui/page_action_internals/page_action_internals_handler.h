// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_HANDLER_H_

#include "chrome/browser/ui/webui/page_action_internals/page_action_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace page_actions {
class PageActionController;
}

class PageActionInternalsHandler
    : public page_action_internals::mojom::PageHandler {
 public:
  explicit PageActionInternalsHandler(
      mojo::PendingReceiver<page_action_internals::mojom::PageHandler>
          receiver);

  PageActionInternalsHandler(const PageActionInternalsHandler&) = delete;
  PageActionInternalsHandler& operator=(const PageActionInternalsHandler&) =
      delete;

  ~PageActionInternalsHandler() override;

  // page_action_internals::mojom::PageHandler:
  void ShowPageAction(page_action_internals::mojom::PageActionParamsPtr params,
                      ShowPageActionCallback callback) override;
  void ShowAnchoredMessage(
      page_action_internals::mojom::AnchoredMessageParamsPtr params,
      ShowAnchoredMessageCallback callback) override;
  void HidePageAction(HidePageActionCallback callback) override;

 private:
  // Helper to get the active PageActionController.
  page_actions::PageActionController* GetActiveController();

  mojo::Receiver<page_action_internals::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_HANDLER_H_
