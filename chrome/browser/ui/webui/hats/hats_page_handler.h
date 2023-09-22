// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HATS_HATS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HATS_HATS_PAGE_HANDLER_H_

#include "chrome/browser/ui/webui/hats/hats.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class HatsPageHandler : public hats::mojom::PageHandler {
 public:
  HatsPageHandler(mojo::PendingReceiver<hats::mojom::PageHandler> receiver,
                  mojo::PendingRemote<hats::mojom::Page> page);

  HatsPageHandler(const HatsPageHandler&) = delete;
  HatsPageHandler& operator=(const HatsPageHandler&) = delete;

  ~HatsPageHandler() override;

  // hats::mojom::PageHandler:
  void GetApiKey(GetApiKeyCallback callback) override;

 private:
  mojo::Receiver<hats::mojom::PageHandler> receiver_;
  mojo::Remote<hats::mojom::Page> page_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HATS_HATS_PAGE_HANDLER_H_
