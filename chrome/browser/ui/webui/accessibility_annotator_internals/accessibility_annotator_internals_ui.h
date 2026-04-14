// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_INTERNALS_ACCESSIBILITY_ANNOTATOR_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_INTERNALS_ACCESSIBILITY_ANNOTATOR_INTERNALS_UI_H_

#include "chrome/browser/ui/webui/accessibility_annotator_internals/accessibility_annotator_internals.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class AccessibilityAnnotatorInternalsPageHandler;
class AccessibilityAnnotatorInternalsUI;

class AccessibilityAnnotatorInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<
          AccessibilityAnnotatorInternalsUI> {
 public:
  AccessibilityAnnotatorInternalsUIConfig()
      : DefaultInternalWebUIConfig(
            chrome::kChromeUIAccessibilityAnnotatorInternalsHost) {}
};

class AccessibilityAnnotatorInternalsUI
    : public ui::MojoWebUIController,
      public browser::accessibility_annotator_internals::mojom::
          PageHandlerFactory {
 public:
  explicit AccessibilityAnnotatorInternalsUI(content::WebUI* web_ui);
  ~AccessibilityAnnotatorInternalsUI() override;

  AccessibilityAnnotatorInternalsUI(const AccessibilityAnnotatorInternalsUI&) =
      delete;
  AccessibilityAnnotatorInternalsUI& operator=(
      const AccessibilityAnnotatorInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<
          browser::accessibility_annotator_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // browser::accessibility_annotator_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<
          browser::accessibility_annotator_internals::mojom::PageHandler>
          handler) override;

  std::unique_ptr<AccessibilityAnnotatorInternalsPageHandler> page_handler_;
  mojo::Receiver<
      browser::accessibility_annotator_internals::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_INTERNALS_ACCESSIBILITY_ANNOTATOR_INTERNALS_UI_H_
