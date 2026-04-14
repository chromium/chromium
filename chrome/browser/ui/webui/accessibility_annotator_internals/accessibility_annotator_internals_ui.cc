// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator_internals/accessibility_annotator_internals_ui.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/accessibility_annotator_internals/accessibility_annotator_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/accessibility_annotator_internals_resources.h"
#include "chrome/grit/accessibility_annotator_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

AccessibilityAnnotatorInternalsUI::AccessibilityAnnotatorInternalsUI(
    content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIAccessibilityAnnotatorInternalsHost);

  webui::SetupWebUIDataSource(
      source, base::span(kAccessibilityAnnotatorInternalsResources),
      IDR_ACCESSIBILITY_ANNOTATOR_INTERNALS_ACCESSIBILITY_ANNOTATOR_INTERNALS_HTML);
}

AccessibilityAnnotatorInternalsUI::~AccessibilityAnnotatorInternalsUI() =
    default;

void AccessibilityAnnotatorInternalsUI::BindInterface(
    mojo::PendingReceiver<
        browser::accessibility_annotator_internals::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void AccessibilityAnnotatorInternalsUI::CreatePageHandler(
    mojo::PendingReceiver<
        browser::accessibility_annotator_internals::mojom::PageHandler>
        handler) {
  page_handler_ = std::make_unique<AccessibilityAnnotatorInternalsPageHandler>(
      std::move(handler), Profile::FromWebUI(web_ui()),
      web_ui()->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(AccessibilityAnnotatorInternalsUI)
