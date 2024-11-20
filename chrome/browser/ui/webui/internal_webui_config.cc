// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internal_webui_config.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/internal_debug_pages_disabled/internal_debug_pages_disabled_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace webui {

InternalWebUIConfig::InternalWebUIConfig(std::string_view scheme)
    : WebUIConfig(content::kChromeUIScheme, scheme) {}

InternalWebUIConfig::~InternalWebUIConfig() = default;

std::unique_ptr<content::WebUIController>
InternalWebUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                           const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kInternalOnlyUisPref)) {
    return nullptr;
  }

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  DCHECK(local_state->FindPreference(prefs::kInternalOnlyUisEnabled));
  if (local_state->GetBoolean(prefs::kInternalOnlyUisEnabled)) {
    return nullptr;
  }

  return std::make_unique<InternalDebugPagesDisabledUI>(web_ui, url.host());
}

}  // namespace webui
