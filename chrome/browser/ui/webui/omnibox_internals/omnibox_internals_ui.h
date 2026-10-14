// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_INTERNALS_OMNIBOX_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_INTERNALS_OMNIBOX_INTERNALS_UI_H_

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/omnibox_internals/aim_eligibility/aim_eligibility.mojom.h"
#include "chrome/browser/ui/webui/omnibox_internals/logging/logs.mojom.h"
#include "chrome/browser/ui/webui/omnibox_internals/omnibox_internals.mojom-forward.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class AimEligibilityPageHandler;
class Profile;

namespace omnibox::logging {
class LogsPageHandler;
}  // namespace omnibox::logging

class OmniboxInternalsPageHandler;

class OmniboxInternalsUI;

class OmniboxInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<OmniboxInternalsUI> {
 public:
  OmniboxInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIOmniboxInternalsHost) {}

  // content::WebUIConfig:
  bool SupportsInProcessResourceLoadingV2() const override;
  bool ShouldCrashOnJavascriptErrorInDevelopmentBuild() const override;
};

// The UI for chrome://omnibox-internals/
class OmniboxInternalsUI : public ui::MojoWebUIController,
                           public aim_eligibility::mojom::PageHandlerFactory,
                           public omnibox::logging::mojom::PageHandlerFactory {
 public:
  explicit OmniboxInternalsUI(content::WebUI* contents);

  OmniboxInternalsUI(const OmniboxInternalsUI&) = delete;
  OmniboxInternalsUI& operator=(const OmniboxInternalsUI&) = delete;

  ~OmniboxInternalsUI() override;

  static base::DictValue GetAimEligibilityLoadTimeData(Profile* profile);

  // Instantiates the implementor of the mojom::OmniboxInternalsPageHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::OmniboxInternalsPageHandler> receiver);

  void BindInterface(
      mojo::PendingReceiver<aim_eligibility::mojom::PageHandlerFactory>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<omnibox::logging::mojom::PageHandlerFactory>
          receiver);

  // aim_eligibility::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<aim_eligibility::mojom::Page> page,
      mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> handler)
      override;

  // omnibox::logging::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<omnibox::logging::mojom::Page> page,
      mojo::PendingReceiver<omnibox::logging::mojom::PageHandler> handler)
      override;

 private:
  std::unique_ptr<OmniboxInternalsPageHandler> omnibox_handler_;
  std::unique_ptr<AimEligibilityPageHandler> aim_eligibility_page_handler_;
  mojo::Receiver<aim_eligibility::mojom::PageHandlerFactory>
      aim_eligibility_factory_receiver_{this};

  std::unique_ptr<omnibox::logging::LogsPageHandler> logs_page_handler_;
  mojo::Receiver<omnibox::logging::mojom::PageHandlerFactory>
      logs_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_INTERNALS_OMNIBOX_INTERNALS_UI_H_
