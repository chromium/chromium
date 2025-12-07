// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"

class DataSharingPageHandler;
class DataSharingUI;

class DataSharingUIConfig : public DefaultTopChromeWebUIConfig<DataSharingUI> {
 public:
  DataSharingUIConfig();
  ~DataSharingUIConfig() override;

  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldAutoResizeHost() override;
};

class DataSharingUI : public UntrustedTopChromeWebUIController,
                      public data_sharing::mojom::PageHandlerFactory {
 public:
  // Delegate for the DataSharingUI.
  class Delegate {
   public:
    // Called when the api is fully initialized and authenticated.
    virtual void ApiInitComplete() = 0;

    // Called to show the error dialog when an error is occurred.
    virtual void ShowErrorDialog(int status_code) = 0;

    virtual void OnShareLinkRequested(
        const std::string& group_id,
        const std::string& access_token,
        base::OnceCallback<void(const std::optional<GURL>&)> callback) = 0;

    virtual void OnGroupAction(
        data_sharing::mojom::GroupAction action,
        data_sharing::mojom::GroupActionProgress progress) = 0;
  };
  explicit DataSharingUI(content::WebUI* web_ui);
  ~DataSharingUI() override;

  void BindInterface(
      mojo::PendingReceiver<data_sharing::mojom::PageHandlerFactory> receiver);

  void ApiInitComplete();

  bool IsApiInitialized();

  void OnShareLinkRequested(
      const std::string& group_id,
      const std::string& access_token,
      base::OnceCallback<void(const std::optional<GURL>&)> callback);

  void OnGroupAction(data_sharing::mojom::GroupAction action,
                     data_sharing::mojom::GroupActionProgress progress);

  void ShowErrorDialog(int status_code);

  DataSharingPageHandler* page_handler() { return page_handler_.get(); }

  static constexpr std::string_view GetWebUIName() {
    return "DataSharingBubble";
  }

  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  // data_sharing::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<data_sharing::mojom::Page> page,
                         mojo::PendingReceiver<data_sharing::mojom::PageHandler>
                             receiver) override;

  std::unique_ptr<DataSharingPageHandler> page_handler_;

  mojo::Receiver<data_sharing::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  raw_ptr<Delegate> delegate_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_UI_H_
