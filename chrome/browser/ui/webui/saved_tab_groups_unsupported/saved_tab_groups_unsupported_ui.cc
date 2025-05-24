// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/saved_tab_groups_unsupported/saved_tab_groups_unsupported_ui.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/grit/saved_tab_groups_unsupported_resources.h"
#include "chrome/grit/saved_tab_groups_unsupported_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"

SavedTabGroupsUnsupportedUI::SavedTabGroupsUnsupportedUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUISavedTabGroupsUnsupportedHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, kSavedTabGroupsUnsupportedResources,
      IDR_SAVED_TAB_GROUPS_UNSUPPORTED_UNSUPPORTED_HTML);

  source->AddString(
      "errorMessage",
      l10n_util::GetStringUTF16(IDS_SAVED_TAB_GROUPS_UNSUPPORTED_ERROR));
}

SavedTabGroupsUnsupportedUI::~SavedTabGroupsUnsupportedUI() = default;
