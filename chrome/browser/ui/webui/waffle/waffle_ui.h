// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WAFFLE_WAFFLE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WAFFLE_WAFFLE_UI_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/waffle/waffle.mojom.h"
#include "chrome/browser/ui/webui/waffle/waffle_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"

// The WebUI controller for `chrome://waffle`.
class WaffleUI : public ui::MojoWebUIController,
                 public waffle::mojom::PageHandlerFactory {
 public:
  explicit WaffleUI(content::WebUI* web_ui);

  WaffleUI(const WaffleUI&) = delete;
  WaffleUI& operator=(const WaffleUI&) = delete;

  ~WaffleUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<waffle::mojom::PageHandlerFactory> receiver);

  // Initializes the callbacks that need to be passed to the handler.
  // `display_dialog_callback` is how we display the waffle dialog. It will
  // be called when the page content is laid out, so that the dialog will be
  // able to measure the page to fit to its size.
  void Initialize(base::OnceClosure display_dialog_callback);

 private:
  // waffle::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<waffle::mojom::PageHandler> receiver) override;

  std::unique_ptr<WaffleHandler> page_handler_;

  mojo::Receiver<waffle::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  base::OnceClosure display_dialog_callback_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WAFFLE_WAFFLE_UI_H_
