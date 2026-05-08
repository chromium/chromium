// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_page_handler.h"
#include "chrome/grit/accessibility_annotator_info_resources.h"
#include "chrome/grit/accessibility_annotator_info_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "components/strings/grit/components_variant_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

namespace accessibility_annotator::info {

AccessibilityAnnotatorInfoUIConfig::AccessibilityAnnotatorInfoUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  "accessibility-annotator-info") {}

bool AccessibilityAnnotatorInfoUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return true;
}

bool AccessibilityAnnotatorInfoUIConfig::ShouldAutoResizeHost() {
  return true;
}

AccessibilityAnnotatorInfoUI::AccessibilityAnnotatorInfoUI(
    content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      "accessibility-annotator-info");

  webui::SetupWebUIDataSource(
      source, kAccessibilityAnnotatorInfoResources,
      IDR_ACCESSIBILITY_ANNOTATOR_INFO_ACCESSIBILITY_ANNOTATOR_INFO_HTML);
  source->AddLocalizedString("accessibilityAnnotatorInfoTitle",
                             IDS_ACCESSIBILITY_ANNOTATOR_INFO_TITLE);
  source->AddLocalizedString(
      "accessibilityAnnotatorInfoDescription",
      IDS_ACCESSIBILITY_ANNOTATOR_INFO_DESCRIPTION_DESKTOP);
  source->AddLocalizedString("accessibilityAnnotatorInfoCard1",
                             IDS_ACCESSIBILITY_ANNOTATOR_INFO_CARD_1_DESKTOP);
  source->AddLocalizedString("accessibilityAnnotatorInfoCard2",
                             IDS_ACCESSIBILITY_ANNOTATOR_INFO_CARD_2_DESKTOP);
  source->AddLocalizedString(
      "accessibilityAnnotatorInfoLearnMore",
      IDS_ACCESSIBILITY_ANNOTATOR_INFO_LEARN_MORE_DESKTOP);
  source->AddLocalizedString("accessibilityAnnotatorInfoPrimaryButton",
                             IDS_ACCESSIBILITY_ANNOTATOR_INFO_PRIMARY_BUTTON);
  source->AddLocalizedString("accessibilityAnnotatorInfoSecondaryButton",
                             IDS_ACCESSIBILITY_ANNOTATOR_INFO_SECONDARY_BUTTON);
  source->AddString(
      "accessibilityAnnotatorTriggerText",
      accessibility_annotator::kAccessibilityAnnotatorTriggerText);
}

AccessibilityAnnotatorInfoUI::~AccessibilityAnnotatorInfoUI() {
  if (dialog_callback_) {
    std::move(dialog_callback_).Run(InfoDialogResult::kDismissed);
  }
}

void AccessibilityAnnotatorInfoUI::BindInterface(
    mojo::PendingReceiver<accessibility_annotator::info::mojom::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<AccessibilityAnnotatorInfoPageHandler>(
      std::move(receiver), std::move(dialog_callback_), *this,
      web_ui()->GetWebContents());
}

void AccessibilityAnnotatorInfoUI::ShowUI() {
  if (embedder()) {
    embedder()->ShowUI();
  }
}

void AccessibilityAnnotatorInfoUI::SetDialogCallback(
    base::OnceCallback<void(InfoDialogResult)> callback) {
  dialog_callback_ = std::move(callback);
}

WEB_UI_CONTROLLER_TYPE_IMPL(AccessibilityAnnotatorInfoUI)

}  // namespace accessibility_annotator::info
