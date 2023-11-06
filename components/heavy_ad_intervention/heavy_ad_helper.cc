// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heavy_ad_intervention/heavy_ad_helper.h"

#include <ostream>

#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace heavy_ad_intervention {

std::string PrepareHeavyAdPage(const std::string& application_locale) {
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SECURITY_INTERSTITIAL_QUIET_HTML);
  DCHECK(!html.empty()) << "unable to load template.";

  // Populate load time data.
  base::Value::Dict load_time_data;
  load_time_data.Set("type", "HEAVYAD");
  load_time_data.Set(
      "heading", l10n_util::GetStringUTF16(IDS_HEAVY_AD_INTERVENTION_HEADING));
  load_time_data.Set(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_HEAVY_AD_INTERVENTION_BUTTON_DETAILS));
  load_time_data.Set(
      "explanationParagraph",
      l10n_util::GetStringUTF16(IDS_HEAVY_AD_INTERVENTION_SUMMARY));

  // Ad frames are never the main frame, so we do not need a tab title.
  load_time_data.Set("tabTitle", "");
  load_time_data.Set("overridable", false);
  load_time_data.Set("is_giant", false);

  webui::SetLoadTimeDataDefaults(application_locale, &load_time_data);

  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetLocalizedHtml(html, load_time_data);
}

}  // namespace heavy_ad_intervention
