// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_ui.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
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
  update->SetBoolean(
      "isConsent", dialog_type == PrivacySandboxService::DialogType::kConsent);
  content::WebUIDataSource::Update(
      profile, chrome::kChromeUIPrivacySandboxDialogHost, std::move(update));

  auto handler = std::make_unique<PrivacySandboxDialogHandler>(
      std::move(close_callback), std::move(resize_callback),
      std::move(open_settings_callback));
  web_ui()->AddMessageHandler(std::move(handler));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PrivacySandboxDialogUI)
