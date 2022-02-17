// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READER_MODE_READER_MODE_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READER_MODE_READER_MODE_SIDE_PANEL_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/read_later/side_panel/reader_mode/reader_mode.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class ReaderModePageHandler;

class ReaderModeSidePanelUI : public ui::MojoBubbleWebUIController,
                              public reader_mode::mojom::PageHandlerFactory {
 public:
  explicit ReaderModeSidePanelUI(content::WebUI* web_ui);
  ReaderModeSidePanelUI(const ReaderModeSidePanelUI&) = delete;
  ReaderModeSidePanelUI& operator=(const ReaderModeSidePanelUI&) = delete;
  ~ReaderModeSidePanelUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<reader_mode::mojom::PageHandlerFactory> receiver);

 private:
  // reader_mode::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<reader_mode::mojom::Page> page,
      mojo::PendingReceiver<reader_mode::mojom::PageHandler> receiver) override;

  std::unique_ptr<ReaderModePageHandler> reader_mode_page_handler_;
  mojo::Receiver<reader_mode::mojom::PageHandlerFactory>
      reader_mode_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_SIDE_PANEL_READER_MODE_READER_MODE_SIDE_PANEL_UI_H_
