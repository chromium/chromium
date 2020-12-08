// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_H_
#define CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_H_

#include <memory>

#include "chromeos/components/media_app_ui/media_app_ui.mojom.h"
#include "chromeos/components/media_app_ui/media_app_ui_delegate.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class MediaAppPageHandler;

namespace chromeos {

// The WebUI controller for chrome://media-app.
class MediaAppUI : public ui::MojoWebUIController,
                   public media_app_ui::mojom::PageHandlerFactory {
 public:
  MediaAppUI(content::WebUI* web_ui,
             std::unique_ptr<MediaAppUIDelegate> delegate);
  ~MediaAppUI() override;

  MediaAppUI(const MediaAppUI&) = delete;
  MediaAppUI& operator=(const MediaAppUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<media_app_ui::mojom::PageHandlerFactory> receiver);
  MediaAppUIDelegate* delegate() { return delegate_.get(); }

  bool IsJavascriptErrorReportingEnabled() override;

 private:
  // media_app_ui::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingReceiver<media_app_ui::mojom::PageHandler>
                             receiver) override;

  std::unique_ptr<MediaAppPageHandler> page_handler_;
  mojo::Receiver<media_app_ui::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  std::unique_ptr<MediaAppUIDelegate> delegate_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MEDIA_APP_UI_MEDIA_APP_UI_H_
