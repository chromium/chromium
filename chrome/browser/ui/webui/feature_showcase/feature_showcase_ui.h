// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/feature_showcase/feature_showcase.mojom.h"
#include "chrome/browser/ui/webui/feature_showcase/password_manager.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

class FeatureShowcaseHandler;
class PasswordManagerHandler;
class FeatureShowcaseUI;

// The WebUIConfig for `chrome://feature-showcase`.
class FeatureShowcaseUIConfig
    : public content::DefaultWebUIConfig<FeatureShowcaseUI> {
 public:
  FeatureShowcaseUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUIController for `chrome://feature-showcase`.
class FeatureShowcaseUI
    : public ui::MojoWebUIController,
      public feature_showcase::mojom::FeatureShowcasePageHandlerFactory,
      public feature_showcase::mojom::PasswordManagerPageHandlerFactory {
 public:
  WEB_UI_CONTROLLER_TYPE_DECL();

  explicit FeatureShowcaseUI(content::WebUI* web_ui);
  FeatureShowcaseUI(const FeatureShowcaseUI&) = delete;
  FeatureShowcaseUI& operator=(const FeatureShowcaseUI&) = delete;
  ~FeatureShowcaseUI() override;

  // Requests the WebUI to show the showcase, and executes `finish_callback`
  // when the user is done.
  void SetFinishCallback(base::OnceClosure finish_callback);

  // Instantiates the implementor of the
  // feature_showcase::mojom::FeatureShowcasePageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          feature_showcase::mojom::FeatureShowcasePageHandlerFactory> receiver);

  // Instantiates the implementor of the
  // feature_showcase::mojom::PasswordManagerPageHandlerFactory mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          feature_showcase::mojom::PasswordManagerPageHandlerFactory> receiver);

 private:
  // feature_showcase::mojom::FeatureShowcasePageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<feature_showcase::mojom::FeatureShowcasePageHandler>
          handler) override;

  // feature_showcase::mojom::PasswordManagerPageHandlerFactory:
  void CreatePasswordManagerPageHandler(
      mojo::PendingReceiver<feature_showcase::mojom::PasswordManagerPageHandler>
          handler) override;

  void OnShowcaseFinished();

  base::OnceClosure finish_callback_;
  std::unique_ptr<FeatureShowcaseHandler> page_handler_;
  std::unique_ptr<PasswordManagerHandler> password_manager_handler_;

  mojo::Receiver<feature_showcase::mojom::FeatureShowcasePageHandlerFactory>
      page_factory_receiver_{this};
  mojo::Receiver<feature_showcase::mojom::PasswordManagerPageHandlerFactory>
      password_manager_factory_receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_
