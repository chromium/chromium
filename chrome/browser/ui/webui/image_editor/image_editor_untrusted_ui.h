// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UNTRUSTED_UI_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/webui/image_editor/image_editor.mojom.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace image_editor {

class ImageEditorUntrustedUIConfig : public content::WebUIConfig {
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
  content::WebUIDataSource* CreateAndAddImageEditorUntrustedDataSource(
      content::WebUI* web_ui);

  void StartLoadFromComponentLoadBytes(
      const std::string& resource_path,
      content::WebUIDataSource::GotDataCallback got_data_callback);

  // mojom::ImageEditorHandler:
  // Send User actions to the browser process.
  void RecordUserAction(mojom::EditAction action) override;

  mojo::Receiver<mojom::ImageEditorHandler> receiver_{this};

  // Background task runner for file I/O.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Temporary filename where screenshot file was stored.
  base::FilePath screenshot_filepath_;

  base::WeakPtrFactory<ImageEditorUntrustedUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace image_editor

#endif  // CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_IMAGE_EDITOR_UNTRUSTED_UI_H_
