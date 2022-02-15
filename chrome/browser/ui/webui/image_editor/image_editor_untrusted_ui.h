// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UNTRUSTED_UI_H_

#include "chrome/browser/ui/webui/image_editor/image_editor.mojom.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

namespace image_editor {

class ImageEditorUntrustedUIConfig : public ui::WebUIConfig {
 public:
  ImageEditorUntrustedUIConfig();
  ~ImageEditorUntrustedUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

class ImageEditorUntrustedUI : public ui::UntrustedWebUIController,
                               public mojom::ImageEditorHandler {
 public:
  explicit ImageEditorUntrustedUI(content::WebUI* web_ui);
  ImageEditorUntrustedUI(const ImageEditorUntrustedUI&) = delete;
  ImageEditorUntrustedUI& operator=(const ImageEditorUntrustedUI&) = delete;
  ~ImageEditorUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::ImageEditorHandler> pending_receiver);

 private:
  // mojom::ImageEditorHandler:

  // Send User actions to the browser process.
  void RecordUserAction(mojom::EditAction action) override;

  mojo::Receiver<mojom::ImageEditorHandler> receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace image_editor

#endif  // CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UNTRUSTED_UI_H_
