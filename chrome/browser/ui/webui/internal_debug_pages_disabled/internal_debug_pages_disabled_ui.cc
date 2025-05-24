// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internal_debug_pages_disabled/internal_debug_pages_disabled_ui.h"

#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/internal_debug_pages_disabled_resources.h"
#include "components/grit/internal_debug_pages_disabled_resources_map.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_util.h"

namespace {

void CreateAndAddHTMLSource(Profile* profile, const std::string& host_name) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIInternalDebugPagesDisabledHost);
  source->AddLocalizedString("pageHeading",
                             IDS_INTERNAL_DEBUG_PAGES_DISABLED_HEADING);
  std::u16string body =
      l10n_util::GetStringFUTF16(IDS_INTERNAL_DEBUG_PAGES_DISABLED_BODY,
                                 base::StrCat({chrome::kChromeUIChromeURLsURL16,
                                               u"#internal-debug-pages"}));
  source->AddString("pageBody", body);

  // All debug UIs have the chrome:// scheme, so just replace the host for
  // chrome://debug-webuis-disabled with the desired host to get a URL. This is
  // used to validate the query parameter is a valid debug WebUI host before
  // passing it to the frontend.
  GURL::Replacements replacements;
  replacements.SetHostStr(host_name);
  GURL url = GURL(chrome::kChromeUIInternalDebugPagesDisabledURL)
                 .ReplaceComponents(replacements);
  source->AddString("host", content::IsInternalWebUI(url) ? host_name : "");

  // Normally, we should only get here if debug pages are disabled. However,
  // forward/back navigation will try to recreate the same WebUI as before, so
  // will reload this one even after the user enables debug pages.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  DCHECK(local_state->FindPreference(chrome_urls::kInternalOnlyUisEnabled));
  source->AddBoolean(
      "debugPagesEnabled",
      local_state->GetBoolean(chrome_urls::kInternalOnlyUisEnabled));

  webui::SetupWebUIDataSource(source, kInternalDebugPagesDisabledResources,
                              IDR_INTERNAL_DEBUG_PAGES_DISABLED_APP_HTML);
}

}  // namespace

InternalDebugPagesDisabledUI::InternalDebugPagesDisabledUI(
    content::WebUI* web_ui,
    const GURL& url)
    : WebUIController(web_ui) {
  // Since we set the query to host=host_name, grab substr(5).
  std::string query = url.query();
  CreateAndAddHTMLSource(Profile::FromWebUI(web_ui),
                         query.length() > 5 ? query.substr(5) : "");
}
