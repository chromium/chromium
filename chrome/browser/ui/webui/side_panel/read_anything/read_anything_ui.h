// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UI_H_

#include <memory>

#include "chrome/common/accessibility/read_anything.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

class ReadAnythingPageHandler;

class ReadAnythingUIUntrustedConfig : public content::WebUIConfig {
 public:
  ReadAnythingUIUntrustedConfig();
  ~ReadAnythingUIUntrustedConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingUI
//
//  A WebUI that holds distilled page contents for the Read Anything feature.
//  This class has the same lifetime as the Side Panel view.
//
class ReadAnythingUI : public ui::UntrustedBubbleWebUIController,
                       public read_anything::mojom::PageHandlerFactory {
 public:
  explicit ReadAnythingUI(content::WebUI* web_ui);
  ReadAnythingUI(const ReadAnythingUI&) = delete;
  ReadAnythingUI& operator=(const ReadAnythingUI&) = delete;
  ~ReadAnythingUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<read_anything::mojom::PageHandlerFactory> receiver);

 private:
  // read_anything::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<read_anything::mojom::Page> page,
      mojo::PendingReceiver<read_anything::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<ReadAnythingPageHandler> read_anything_page_handler_;
  mojo::Receiver<read_anything::mojom::PageHandlerFactory>
      read_anything_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_UI_H_
