// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_BATTERY_SAVER_CARD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_PERFORMANCE_CONTROLS_BATTERY_SAVER_CARD_HANDLER_H_

#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class BatterySaverCardHandler
    : public side_panel::mojom::BatterySaverCardHandler {
 public:
  explicit BatterySaverCardHandler(
      mojo::PendingReceiver<side_panel::mojom::BatterySaverCardHandler>
          receiver,
      mojo::PendingRemote<side_panel::mojom::BatterySaverCard>
          battery_saver_card);
  BatterySaverCardHandler(const BatterySaverCardHandler&) = delete;
  BatterySaverCardHandler& operator=(const BatterySaverCardHandler&) = delete;
  ~BatterySaverCardHandler() override;

 private:
  mojo::Receiver<side_panel::mojom::BatterySaverCardHandler> receiver_;
  mojo::Remote<side_panel::mojom::BatterySaverCard> battery_saver_card_;
};

#endif
