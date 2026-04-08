// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_page_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace accessibility_annotator::info {

AccessibilityAnnotatorInfoUI::AccessibilityAnnotatorInfoUI(
    content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      "accessibility-annotator-info");
  (void)source;
}

AccessibilityAnnotatorInfoUI::~AccessibilityAnnotatorInfoUI() = default;

void AccessibilityAnnotatorInfoUI::BindInterface(
    mojo::PendingReceiver<accessibility_annotator::info::mojom::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<AccessibilityAnnotatorInfoPageHandler>(
      std::move(receiver), std::move(dialog_callback_),
      web_ui()->GetWebContents()->GetBrowserContext());
}

void AccessibilityAnnotatorInfoUI::SetDialogCallback(
    base::OnceCallback<void(InfoDialogResult)> callback) {
  dialog_callback_ = std::move(callback);
}

WEB_UI_CONTROLLER_TYPE_IMPL(AccessibilityAnnotatorInfoUI)

}  // namespace accessibility_annotator::info
