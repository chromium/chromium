// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class OmniboxPopupHandler : public omnibox_popup::mojom::PageHandler {
 public:
  OmniboxPopupHandler(
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver,
      mojo::PendingRemote<omnibox_popup::mojom::Page> page);

  OmniboxPopupHandler(const OmniboxPopupHandler&) = delete;
  OmniboxPopupHandler& operator=(const OmniboxPopupHandler&) = delete;

  ~OmniboxPopupHandler() override;

  // Forwards an `OnShow()` call to the page.
  void OnShow();

 private:
  mojo::Receiver<omnibox_popup::mojom::PageHandler> receiver_;
  mojo::Remote<omnibox_popup::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_OMNIBOX_POPUP_HANDLER_H_
