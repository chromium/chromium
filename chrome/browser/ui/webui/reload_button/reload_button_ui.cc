// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

// TODO(crbug.com/444358999): implement the reload button
ReloadButtonUI::ReloadButtonUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {}

ReloadButtonUI::~ReloadButtonUI() = default;

bool ReloadButtonUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kInitialWebUI) &&
         base::FeatureList::IsEnabled(features::kWebUIReloadButton);
}
