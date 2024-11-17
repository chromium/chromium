// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/discards/discards.mojom-forward.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class DiscardsUI;

class DiscardsUIConfig : public content::DefaultWebUIConfig<DiscardsUI> {
 public:
  DiscardsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIDiscardsHost) {}
};

// Controller for chrome://discards. Corresponding resources are in
// file://chrome/browser/resources/discards.
class DiscardsUI : public ui::MojoWebUIController {
 public:
  explicit DiscardsUI(content::WebUI* web_ui);

  DiscardsUI(const DiscardsUI&) = delete;
  DiscardsUI& operator=(const DiscardsUI&) = delete;

  ~DiscardsUI() override;

  // Instantiates the implementor of the mojom::DetailsProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<discards::mojom::DetailsProvider> receiver);

  // Instantiates the implementor of the mojom::SiteDataProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<discards::mojom::SiteDataProvider> receiver);

  // Instantiates the implementor of the mojom::GraphDump mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<discards::mojom::GraphDump> receiver);

 private:
  std::unique_ptr<discards::mojom::DetailsProvider> ui_handler_;
  std::string profile_id_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_
