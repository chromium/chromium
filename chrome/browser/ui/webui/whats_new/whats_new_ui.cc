// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"

#include "base/feature_list.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/whats_new_resources.h"
#include "chrome/grit/whats_new_resources_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

content::WebUIDataSource* CreateWhatsNewUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIWhatsNewHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kWhatsNewResources, kWhatsNewResourcesSize),
      IDR_WHATS_NEW_WHATS_NEW_HTML);
  static constexpr webui::LocalizedString kStrings[] = {
      {"pageCantBeReached", IDS_WHATS_NEW_PAGE_CANT_BE_REACHED},
      {"reloadOrTryAgain", IDS_WHATS_NEW_RELOAD_TRY_AGAIN},
      {"reloadButton", IDS_RELOAD},
  };
  source->AddLocalizedStrings(kStrings);

  return source;
}

}  // namespace

// static
void WhatsNewUI::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kLastWhatsNewVersion, 0);
}

// static
bool WhatsNewUI::ShouldShowForState(PrefService* local_state) {
  if (!local_state)
    return false;

  if (!base::FeatureList::IsEnabled(features::kChromeWhatsNewUI))
    return false;

  int last_version = local_state->GetInteger(prefs::kLastWhatsNewVersion);
  return CHROME_VERSION_MAJOR > last_version;
}

// static
void WhatsNewUI::SetLastVersion(PrefService* local_state) {
  if (!local_state) {
    return;
  }

  local_state->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
}

WhatsNewUI::WhatsNewUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::WebUIDataSource* source =
      CreateWhatsNewUIHtmlSource(Profile::FromWebUI(web_ui));
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // TODO(rbpotter): Once we have a way to detect that the content has loaded
  // successfully, update the kLastWhatsNewVersion pref.
}

WhatsNewUI::~WhatsNewUI() = default;
