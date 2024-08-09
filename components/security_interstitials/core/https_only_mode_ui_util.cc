// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/https_only_mode_ui_util.h"

#include "components/feature_engagement/public/feature_list.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

void PopulateHttpsOnlyModeStringsForBlockingPage(
    base::Value::Dict& load_time_data,
    const GURL& url,
    const security_interstitials::https_only_mode::HttpInterstitialState&
        interstitial_state,
    bool august2024_refresh_enabled) {
  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_HTTPS_ONLY_MODE_TITLE));

  int heading_id = IDS_HTTPS_ONLY_MODE_HEADING;
  int primary_paragraph_id = IDS_HTTPS_ONLY_MODE_PRIMARY_PARAGRAPH;
  if (august2024_refresh_enabled) {
    heading_id = IDS_HTTPS_ONLY_BALANCED_MODE_HEADING;
    primary_paragraph_id = IDS_HTTPS_ONLY_BALANCED_MODE_PRIMARY_PARAGRAPH;
  }

  // Multiple interstitial flags might be true here, but we assign higher
  // priority to Site Engagement heuristic because we expect SE interstitials
  // to be rare. Advanced Protection locks the HTTPS-First Mode UI setting so
  // it's higher priority than the HFM string as well. The lowest priority is
  // given to HFM-in-Incognito, where we want to use different strings only if
  // the user is not opted in to HFM for any other reason.
  if (interstitial_state.enabled_by_engagement_heuristic) {
    primary_paragraph_id =
        IDS_HTTPS_ONLY_MODE_WITH_SITE_ENGAGEMENT_PRIMARY_PARAGRAPH;
  } else if (interstitial_state.enabled_by_advanced_protection) {
    primary_paragraph_id =
        IDS_HTTPS_ONLY_MODE_WITH_ADVANCED_PROTECTION_PRIMARY_PARAGRAPH;
  } else if (interstitial_state.enabled_by_typically_secure_browsing) {
    primary_paragraph_id =
        IDS_HTTPS_ONLY_MODE_FOR_TYPICALLY_SECURE_BROWSING_PRIMARY_PARAGRAPH;
  } else if (interstitial_state.enabled_by_incognito &&
             !interstitial_state.enabled_by_pref &&
             !interstitial_state.enabled_in_balanced_mode) {
    primary_paragraph_id = IDS_HTTPS_ONLY_MODE_FOR_INCOGNITO_PRIMARY_PARAGRAPH;
  }

  // TODO(crbug.com/349860796): Consider customizing interstitial strings for
  // balanced mode.
  load_time_data.Set(
      "heading",
      l10n_util::GetStringFUTF16(
          heading_id,
          security_interstitials::common_string_util::GetFormattedHostName(
              url)));
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringUTF16(primary_paragraph_id));

  // TODO(crbug.com/40825375): Change this button to "Close" when we can't go
  // back:
  load_time_data.Set(
      "proceedButtonText",
      l10n_util::GetStringUTF16(IDS_HTTPS_ONLY_MODE_SUBMIT_BUTTON));
  load_time_data.Set("primaryButtonText", l10n_util::GetStringUTF16(
                                              IDS_HTTPS_ONLY_MODE_BACK_BUTTON));
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));
}

void PopulateHttpsOnlyModeStringsForSharedHTML(
    base::Value::Dict& load_time_data,
    bool august2024_refresh_enabled) {
  load_time_data.Set("type", "HTTPS_ONLY");
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
  if (august2024_refresh_enabled) {
    load_time_data.Set("august2024Refresh", true);
  }
}
