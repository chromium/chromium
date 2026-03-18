// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_UI_H_

#include <memory>

#include "chrome/common/webui_url_constants.h"
#include "components/accessibility_annotator/core/logging/accessibility_annotator_internals.mojom.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content_annotator_internals {

class ContentAnnotatorInternalsUI;
class ContentAnnotatorInternalsPageHandler;

// The WebUIConfig for chrome://content-annotator-internals
class ContentAnnotatorInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<ContentAnnotatorInternalsUI> {
 public:
  ContentAnnotatorInternalsUIConfig()
      : DefaultInternalWebUIConfig(
            chrome::kChromeUIContentAnnotatorInternalsHost) {}
};

// The WebUIController for chrome://content-annotator-internals
class ContentAnnotatorInternalsUI
    : public ui::MojoWebUIController,
      public accessibility_annotator_internals::mojom::PageHandlerFactory {
 public:
  explicit ContentAnnotatorInternalsUI(content::WebUI* web_ui);
  ContentAnnotatorInternalsUI(const ContentAnnotatorInternalsUI&) = delete;
  ContentAnnotatorInternalsUI& operator=(const ContentAnnotatorInternalsUI&) =
      delete;
  ~ContentAnnotatorInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          accessibility_annotator_internals::mojom::PageHandlerFactory>
          receiver);

  // accessibility_annotator_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<accessibility_annotator_internals::mojom::Page> page,
      mojo::PendingReceiver<
          accessibility_annotator_internals::mojom::PageHandler> receiver)
      override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<ContentAnnotatorInternalsPageHandler> page_handler_;
  mojo::Receiver<accessibility_annotator_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
};

}  // namespace content_annotator_internals

#endif  // CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_UI_H_
