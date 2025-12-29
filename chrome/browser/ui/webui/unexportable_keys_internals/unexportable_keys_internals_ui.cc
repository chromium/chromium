// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/unexportable_keys_internals_resources.h"
#include "chrome/grit/unexportable_keys_internals_resources_map.h"
#include "components/unexportable_keys/features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

bool UnexportableKeysInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      unexportable_keys::kUnexportableKeyDeletion);
}

UnexportableKeysInternalsUI::UnexportableKeysInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIUnexportableKeysInternalsHost);
  webui::SetupWebUIDataSource(
      source, kUnexportableKeysInternalsResources,
      IDR_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_HTML);
  source->AddString("message", "Hello, World!");
}

UnexportableKeysInternalsUI::~UnexportableKeysInternalsUI() = default;
