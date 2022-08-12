// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_UI_H_

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_page_handler.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos::cloud_upload {

// The UI for chrome://cloud-upload, used for uploading files to the cloud.
class CloudUploadUI : public ui::MojoWebDialogUI,
                      public chromeos::cloud_upload::mojom::PageHandlerFactory {
 public:
  explicit CloudUploadUI(content::WebUI* web_ui);
  CloudUploadUI(const CloudUploadUI&) = delete;
  CloudUploadUI& operator=(const CloudUploadUI&) = delete;

  ~CloudUploadUI() override;

  // Instantiates implementor of the mojom::PageHandlerFactory
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::cloud_upload::mojom::PageHandlerFactory>
          pending_receiver);

  // chromeos::cloud_upload::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<chromeos::cloud_upload::mojom::PageHandler>
          pending_page_handler) override;

 private:
  void RespondAndCloseDialog(mojom::UserAction action);

  std::unique_ptr<CloudUploadPageHandler> page_handler_;
  mojo::Receiver<chromeos::cloud_upload::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_UI_H_
