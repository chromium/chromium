// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_HANDLER_H_

#include "chrome/browser/ui/webui/page_action_internals/page_action_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

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

 private:
  mojo::Receiver<page_action_internals::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PAGE_ACTION_INTERNALS_PAGE_ACTION_INTERNALS_HANDLER_H_
