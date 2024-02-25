// Copyright 2020 The Chromium Authors
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

namespace lookalikes {

void RecordUkmForLookalikeUrlBlockingPage(
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type,
    LookalikeUrlBlockingPageUserAction user_action,
    bool triggered_by_initial_url) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  CHECK(ukm_recorder);

  ukm::builders::LookalikeUrl_NavigationSuggestion(source_id)
      .SetMatchType(static_cast<int>(match_type))
      .SetUserAction(static_cast<int>(user_action))
      .SetTriggeredByInitialUrl(static_cast<int>(triggered_by_initial_url))
      .Record(ukm_recorder);
}

void ReportUkmForLookalikeUrlBlockingPageIfNeeded(
    ukm::SourceId& source_id,
    LookalikeUrlMatchType match_type,
    LookalikeUrlBlockingPageUserAction action,
    bool triggered_by_initial_url) {
  // Rely on the saved SourceId because deconstruction happens after the next
  // navigation occurs, so web contents points to the new destination.
  if (source_id != ukm::kInvalidSourceId) {
    RecordUkmForLookalikeUrlBlockingPage(source_id, match_type, action,
                                         triggered_by_initial_url);
    source_id = ukm::kInvalidSourceId;
  }
}

void PopulateLookalikeUrlBlockingPageStrings(base::Value::Dict& load_time_data,
                                             const GURL& safe_url,
                                             const GURL& request_url) {
  PopulateStringsForSharedHTML(load_time_data);
  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_TITLE));
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));

  if (safe_url.is_valid()) {
    const std::u16string hostname =
        security_interstitials::common_string_util::GetFormattedHostName(
            safe_url);
    load_time_data.Set("heading", l10n_util::GetStringFUTF16(
                                      IDS_LOOKALIKE_URL_HEADING, hostname));
    load_time_data.Set(
        "primaryParagraph",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH));
    load_time_data.Set("proceedButtonText",
                       l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_IGNORE));
    load_time_data.Set(
        "primaryButtonText",
        l10n_util::GetStringFUTF16(IDS_LOOKALIKE_URL_CONTINUE, hostname));
  } else {
    // No safe URL available to suggest. This can happen when the navigated
    // domain fails IDN spoof checks but isn't a lookalike of a known domain.
    // TODO: Change to actual strings.
    load_time_data.Set(
        "heading",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_HEADING_NO_SUGGESTED_URL));
    load_time_data.Set(
        "primaryParagraph",
        l10n_util::GetStringUTF16(
            IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH_NO_SUGGESTED_URL));
    load_time_data.Set("proceedButtonText",
                       l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_IGNORE));
    load_time_data.Set(
        "primaryButtonText",
        l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_BACK_TO_SAFETY));
#if BUILDFLAG(IS_IOS)
    // On iOS, offer to close the page instead of navigating to NTP when the
    // safe URL is empty or invalid, and unable to go back.
    std::optional<bool> maybe_cant_go_back =
        load_time_data.FindBool("cant_go_back");
    if (maybe_cant_go_back && *maybe_cant_go_back) {
      load_time_data.Set(
          "primaryButtonText",
          l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_CLOSE_PAGE));
    }
#endif
  }
  load_time_data.Set(
      "lookalikeConsoleMessage",
      lookalikes::GetConsoleMessage(request_url, /*is_new_heuristic=*/false));
}

void PopulateStringsForSharedHTML(base::Value::Dict& load_time_data) {
  load_time_data.Set("lookalike_url", true);
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);

  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");

  load_time_data.Set("type", "LOOKALIKE");
}

}  // namespace lookalikes
