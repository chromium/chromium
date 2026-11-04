// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_utils.h"

#include "base/check.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/interaction/element_tracker.h"
#include "url/gurl.h"

namespace settings_utils {

bool FixupAndValidateStartupPage(const std::string& url_string,
                                 GURL* fixed_url) {
  GURL url = url_formatter::FixupURL(url_string);
  bool valid = url.is_valid() && !extensions::ExtensionTabUtil::IsKillURL(url);
  if (valid && fixed_url) {
    fixed_url->Swap(&url);
  }
  return valid;
}

void MaybeNotifySettingsCustomEvent(content::WebContents* web_contents,
                                    ui::CustomElementEventType event_type) {
  if (!base::FeatureList::IsEnabled(metrics::kSettingsGlowupJourneys)) {
    return;
  }
  // Unit tests (e.g., `SiteSettingsHandlerTest`, `SearchEnginesHandlerTest`)
  // run with a detached `TestWebUI` without a `WebContents` or
  // `BrowserWindowInterface`.
  if (!web_contents) {
    return;
  }
  BrowserWindowInterface* const browser =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser) {
    return;
  }
  BrowserElements* const browser_elements = BrowserElements::From(browser);
  if (!browser_elements) {
    return;
  }
  browser_elements->NotifyEvent(kBrowserViewElementId, event_type);
}

void MaybeNotifySettingsCustomEvent(content::WebUI* web_ui,
                                    ui::CustomElementEventType event_type) {
  CHECK(web_ui);
  MaybeNotifySettingsCustomEvent(web_ui->GetWebContents(), event_type);
}

}  // namespace settings_utils
