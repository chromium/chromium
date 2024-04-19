// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_TOOLBAR_CUSTOMIZE_TOOLBAR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_TOOLBAR_CUSTOMIZE_TOOLBAR_HANDLER_H_

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class CustomizeToolbarHandler
    : public side_panel::customize_chrome::mojom::CustomizeToolbarHandler {
 public:
  CustomizeToolbarHandler(
      mojo::PendingReceiver<
          side_panel::customize_chrome::mojom::CustomizeToolbarHandler> handler,
      mojo::PendingRemote<
          side_panel::customize_chrome::mojom::CustomizeToolbarClient> clent);

  CustomizeToolbarHandler(const CustomizeToolbarHandler&) = delete;
  CustomizeToolbarHandler& operator=(const CustomizeToolbarHandler&) = delete;

  ~CustomizeToolbarHandler() override;

 private:
  mojo::Remote<side_panel::customize_chrome::mojom::CustomizeToolbarClient>
      client_;
  mojo::Receiver<side_panel::customize_chrome::mojom::CustomizeToolbarHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_TOOLBAR_CUSTOMIZE_TOOLBAR_HANDLER_H_
