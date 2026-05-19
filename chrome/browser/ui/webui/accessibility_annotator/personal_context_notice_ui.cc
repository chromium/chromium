// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice_page_handler.h"
#include "chrome/grit/accessibility_annotator_info_resources.h"
#include "chrome/grit/accessibility_annotator_info_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

namespace personal_context::notice {

PersonalContextNoticeUIConfig::PersonalContextNoticeUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  "accessibility-annotator-info") {}

bool PersonalContextNoticeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return true;
}

bool PersonalContextNoticeUIConfig::ShouldAutoResizeHost() {
  return true;
}

PersonalContextNoticeUI::PersonalContextNoticeUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      "accessibility-annotator-info");

  webui::SetupWebUIDataSource(
      source, kAccessibilityAnnotatorInfoResources,
      IDR_ACCESSIBILITY_ANNOTATOR_INFO_PERSONAL_CONTEXT_NOTICE_HTML);
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

PersonalContextNoticeUI::~PersonalContextNoticeUI() {
  if (dialog_callback_) {
    std::move(dialog_callback_).Run(NoticeDialogResult::kDismissed);
  }
}

void PersonalContextNoticeUI::BindInterface(
    mojo::PendingReceiver<personal_context::notice::mojom::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<PersonalContextNoticePageHandler>(
      std::move(receiver), std::move(dialog_callback_), *this,
      web_ui()->GetWebContents());
}

void PersonalContextNoticeUI::ShowUI() {
  if (embedder()) {
    embedder()->ShowUI();
  }
}

void PersonalContextNoticeUI::SetDialogCallback(
    base::OnceCallback<void(NoticeDialogResult)> callback) {
  dialog_callback_ = std::move(callback);
}

WEB_UI_CONTROLLER_TYPE_IMPL(PersonalContextNoticeUI)

}  // namespace personal_context::notice
