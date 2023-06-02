// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WAFFLE_WAFFLE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WAFFLE_WAFFLE_HANDLER_H_

#include "chrome/browser/ui/webui/waffle/waffle.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class WaffleHandler : public waffle::mojom::PageHandler {
 public:
  explicit WaffleHandler(
      mojo::PendingReceiver<waffle::mojom::PageHandler> receiver);

  WaffleHandler(const WaffleHandler&) = delete;
  WaffleHandler& operator=(const WaffleHandler&) = delete;

  ~WaffleHandler() override;

  // waffle::mojom::PageHandler:
  void CloseClicked() override;

 private:
  mojo::Receiver<waffle::mojom::PageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WAFFLE_WAFFLE_HANDLER_H_
