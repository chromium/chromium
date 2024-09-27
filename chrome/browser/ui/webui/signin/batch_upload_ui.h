// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_delegate.h"
#include "chrome/browser/ui/webui/signin/batch_upload/batch_upload.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}

struct AccountInfo;
class BatchUploadHandler;
class BatchUploadUI;

class BatchUploadUIConfig : public content::DefaultWebUIConfig<BatchUploadUI> {
 public:
  BatchUploadUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBatchUploadHost) {}
};

class BatchUploadUI : public ui::MojoWebUIController,
                      public batch_upload::mojom::PageHandlerFactory {
 public:
  explicit BatchUploadUI(content::WebUI* web_ui);
  ~BatchUploadUI() override;

  BatchUploadUI(const BatchUploadUI&) = delete;
  BatchUploadUI& operator=(const BatchUploadUI&) = delete;

  // Prepares the information to be given to the handler once ready.
  void Initialize(
      const AccountInfo& account_info,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      base::RepeatingCallback<void(int)> update_view_height_callback,
      SelectedDataTypeItemsCallback completion_callback);

  // Clears the state of the UI to avoid keeping data coupled to the
  // browser/profile.
  void Clear();

  // Instantiates the implementor of the
  // `batch_upload::mojom::PageHandlerFactory` mojo interface passing the
  // pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<batch_upload::mojom::PageHandlerFactory> receiver);

 private:
  // batch_upload::mojom::BatchUploadHandlerFactory:
  void CreateBatchUploadHandler(
      mojo::PendingRemote<batch_upload::mojom::Page> page,
      mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver)
      override;

  // Callback awaiting `CreateBatchUploadHandler` to create the handlers with
  // all the needed information to display.
  void OnMojoHandlersReady(
      const AccountInfo& account_info,
      std::vector<raw_ptr<const BatchUploadDataProvider>> data_providers_list,
      base::RepeatingCallback<void(int)> update_view_height_callback,
      SelectedDataTypeItemsCallback completion_callback,
      mojo::PendingRemote<batch_upload::mojom::Page> page,
      mojo::PendingReceiver<batch_upload::mojom::PageHandler> receiver);

  // Callback that temporarily holds the information to be passed onto the
  // handler. The callback is called once the mojo handlers are available.
  base::OnceCallback<void(
      mojo::PendingRemote<batch_upload::mojom::Page>,
      mojo::PendingReceiver<batch_upload::mojom::PageHandler>)>
      initialize_handler_callback_;

  // Handler implementing Mojo interface to communicate with the WebUI.
  std::unique_ptr<BatchUploadHandler> handler_;

  mojo::Receiver<batch_upload::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_BATCH_UPLOAD_UI_H_
