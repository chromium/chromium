// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/read_later/read_later.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class ReadLaterPageHandler;

class ReadLaterUI : public ui::MojoBubbleWebUIController,
                    public read_later::mojom::PageHandlerFactory {
 public:
  explicit ReadLaterUI(content::WebUI* web_ui);
  ReadLaterUI(const ReadLaterUI&) = delete;
  ReadLaterUI& operator=(const ReadLaterUI&) = delete;
  ~ReadLaterUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<read_later::mojom::PageHandlerFactory> receiver);

 private:
  // read_later::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<read_later::mojom::Page> page,
      mojo::PendingReceiver<read_later::mojom::PageHandler> receiver) override;

  std::unique_ptr<ReadLaterPageHandler> page_handler_;

  mojo::Receiver<read_later::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  WebuiLoadTimer webui_load_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_READ_LATER_READ_LATER_UI_H_
