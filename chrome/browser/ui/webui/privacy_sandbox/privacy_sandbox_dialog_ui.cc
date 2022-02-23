// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_ui.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/privacy_sandbox_resources.h"
#include "chrome/grit/privacy_sandbox_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

PrivacySandboxDialogUI::PrivacySandboxDialogUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  auto* source = content::WebUIDataSource::Create(
      chrome::kChromeUIPrivacySandboxDialogHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kPrivacySandboxResources, kPrivacySandboxResourcesSize),
      IDR_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_PRIVACY_SANDBOX_DIALOG_TITLE},
      {"subtitle", IDS_PRIVACY_SANDBOX_DIALOG_SUBTITLE},
      {"bodyHeader1", IDS_PRIVACY_SANDBOX_DIALOG_BODY_HEADER_1},
      {"bodyDescription1", IDS_PRIVACY_SANDBOX_DIALOG_BODY_DESCRIPTION_1},
      {"bodyHeader2", IDS_PRIVACY_SANDBOX_DIALOG_BODY_HEADER_2},
      {"bodyDescription2", IDS_PRIVACY_SANDBOX_DIALOG_BODY_DESCRIPTION_2},
      {"learnMoreLabel", IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_LABEL},
      {"bottomSummary", IDS_PRIVACY_SANDBOX_DIALOG_BOTTOM_SUMMARY},
      {"learnMoreSection1Header",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_1_HEADER},
      {"learnMoreSection1BulletPoint1",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_1_BULLET_POINT_1},
      {"learnMoreSection1BulletPoint2",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_1_BULLET_POINT_2},
      {"learnMoreSection1BulletPoint3",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_1_BULLET_POINT_3},
      {"learnMoreSection2Header",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_2_HEADER},
      {"learnMoreSection2BulletPoint1",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_2_BULLET_POINT_1},
      {"learnMoreSection2BulletPoint2",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_2_BULLET_POINT_2},
      {"learnMoreSection2BulletPoint3",
       IDS_PRIVACY_SANDBOX_DIALOG_LEARN_MORE_SECTION_2_BULLET_POINT_3},
      {"consentAcceptButton", IDS_PRIVACY_SANDBOX_DIALOG_CONSENT_ACCEPT_BUTTON},
      {"consentDeclineButton",
       IDS_PRIVACY_SANDBOX_DIALOG_CONSENT_DECLINE_BUTTON},
      {"noticeAcknowledgeButton",
       IDS_PRIVACY_SANDBOX_DIALOG_NOTICE_ACKNOWLEDGE_BUTTON},
      {"noticeOpenSettingsButton",
       IDS_PRIVACY_SANDBOX_DIALOG_NOTICE_OPEN_SETTINGS_BUTTON},
  };

  source->AddLocalizedStrings(kStrings);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

PrivacySandboxDialogUI::~PrivacySandboxDialogUI() = default;

void PrivacySandboxDialogUI::Initialize(
    Profile* profile,
    base::OnceClosure close_callback,
    base::OnceCallback<void(int)> resize_callback,
    base::OnceClosure open_settings_callback,
    PrivacySandboxService::DialogType dialog_type) {
  std::unique_ptr<base::DictionaryValue> update =
      std::make_unique<base::DictionaryValue>();
  update->SetBoolKey(
      "isConsent", dialog_type == PrivacySandboxService::DialogType::kConsent);
  content::WebUIDataSource::Update(
      profile, chrome::kChromeUIPrivacySandboxDialogHost, std::move(update));

  auto handler = std::make_unique<PrivacySandboxDialogHandler>(
      std::move(close_callback), std::move(resize_callback),
      std::move(open_settings_callback), dialog_type);
  web_ui()->AddMessageHandler(std::move(handler));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrivacySandboxDialogUI)
