// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heavy_ad_intervention/heavy_ad_helper.h"

#include <ostream>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace heavy_ad_intervention {

// NOTE: If adding usage of more strings/resources here, make sure that they
// are allowlisted in //weblayer/grit_{resources, strings}_allowlist.txt;
// otherwise empty data will be used for the new resources/strings in WebLayer.
// For strings there is a partial safeguard as //weblayer's integration tests
// will crash if a new-but-not-allowlisted string is fetched in a codepath that
// the presentation of the heavy ad page in those tests exercises.
std::string PrepareHeavyAdPage(const std::string& application_locale) {
  int resource_id = IDR_SECURITY_INTERSTITIAL_QUIET_HTML;
  std::string uncompressed;
  base::StringPiece template_html(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  if (ui::ResourceBundle::GetSharedInstance().IsGzipped(resource_id)) {
    bool success = compression::GzipUncompress(template_html, &uncompressed);
    DCHECK(success);
    template_html = base::StringPiece(uncompressed);
  }
  DCHECK(!template_html.empty()) << "unable to load template.";

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

  // "body" is the id of the template's root node.
  std::string heavy_ad_html =
      webui::GetTemplatesHtml(template_html, load_time_data, "body");
  webui::AppendWebUiCssTextDefaults(&heavy_ad_html);

  return heavy_ad_html;
}

}  // namespace heavy_ad_intervention
