// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_ui_util.h"

#include "build/build_config.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_strings.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/l10n/l10n_util.h"

void RecordUkmForLookalikeUrlBlockingPage(
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type,
    LookalikeUrlBlockingPageUserAction user_action) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  CHECK(ukm_recorder);

  ukm::builders::LookalikeUrl_NavigationSuggestion(source_id)
      .SetMatchType(static_cast<int>(match_type))
      .SetUserAction(static_cast<int>(user_action))
      .Record(ukm_recorder);
}

void ReportUkmForLookalikeUrlBlockingPageIfNeeded(
    ukm::SourceId& source_id,
    LookalikeUrlMatchType match_type,
    LookalikeUrlBlockingPageUserAction action) {
  // Rely on the saved SourceId because deconstruction happens after the next
  // navigation occurs, so web contents points to the new destination.
  if (source_id != ukm::kInvalidSourceId) {
    RecordUkmForLookalikeUrlBlockingPage(source_id, match_type, action);
    source_id = ukm::kInvalidSourceId;
  }
}

void PopulateLookalikeUrlBlockingPageStrings(
    base::DictionaryValue* load_time_data,
    const GURL& safe_url,
    const GURL& request_url) {
  CHECK(load_time_data);

  PopulateStringsForSharedHTML(load_time_data);
  load_time_data->SetString("tabTitle",
                            l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_TITLE));
  load_time_data->SetString(
      "optInLink",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data->SetString(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));

  if (safe_url.is_valid()) {
    const base::string16 hostname =
        security_interstitials::common_string_util::GetFormattedHostName(
            safe_url);
    load_time_data->SetString(
        "heading",
        l10n_util::GetStringFUTF16(IDS_LOOKALIKE_URL_HEADING, hostname));
    load_time_data->SetString(
        "primaryParagraph",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH));
    load_time_data->SetString(
        "proceedButtonText",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_IGNORE));
    load_time_data->SetString(
        "primaryButtonText",
        l10n_util::GetStringFUTF16(IDS_LOOKALIKE_URL_CONTINUE, hostname));
  } else {
    // No safe URL available to suggest. This can happen when the navigated
    // domain fails IDN spoof checks but isn't a lookalike of a known domain.
    // TODO: Change to actual strings.
    load_time_data->SetString(
        "heading",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_HEADING_NO_SUGGESTED_URL));
    load_time_data->SetString(
        "primaryParagraph",
        l10n_util::GetStringUTF16(
            IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH_NO_SUGGESTED_URL));
    load_time_data->SetString(
        "proceedButtonText",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_IGNORE));
    load_time_data->SetString(
        "primaryButtonText",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_BACK_TO_SAFETY));
#if defined(OS_IOS)
    // On iOS, offer to close the page instead of navigating to NTP when the
    // safe URL is empty or invalid, and unable to go back.
    bool show_close_page = false;
    load_time_data->GetBoolean("cant_go_back", &show_close_page);
    if (show_close_page) {
      load_time_data->SetString(
          "primaryButtonText",
          l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_CLOSE_PAGE));
    }
#endif
  }
  load_time_data->SetString("lookalikeRequestHostname", request_url.host());
}

void PopulateStringsForSharedHTML(base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("lookalike_url", true);
  load_time_data->SetBoolean("overridable", false);
  load_time_data->SetBoolean("hide_primary_button", false);
  load_time_data->SetBoolean("show_recurrent_error_paragraph", false);

  load_time_data->SetString("recurrentErrorParagraph", "");
  load_time_data->SetString("openDetails", "");
  load_time_data->SetString("explanationParagraph", "");
  load_time_data->SetString("finalParagraph", "");

  load_time_data->SetString("type", "LOOKALIKE");
}
