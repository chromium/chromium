// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/contextual_cue_log_event_helpers.h"

#include "components/metrics/version_utils.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace private_insights {

std::optional<std::pair<std::string, std::string>> ExtractFromProtoTab(
    const optimization_guide::proto::Tab& tab) {
  return std::make_pair(tab.url(), tab.title());
}

void PopulateCommonSystemProfile(events::CoarseSystemProfile* system_profile) {
  system_profile->set_chrome_version(
      std::string(version_info::GetVersionNumber()));
  system_profile->set_milestone(
      std::string(version_info::GetMajorVersionNumber()));
  system_profile->set_platform(metrics::GetOperatingSystemName());
}

void PopulateActivePage(const GURL& url,
                        const std::string& title,
                        events::ContextualCueLogEvent::PageInfo* active_page) {
  active_page->set_url(url.spec());
  active_page->set_title(title);
  active_page->set_hostname(url.host());
}

void PopulateCueDetails(
    const optimization_guide::proto::ContextualCue& cue,
    std::string_view cuj,
    std::string_view promoted_feature,
    events::ContextualCueLogEvent::CueDetails* cue_details) {
  cue_details->set_cuj_type(std::string(cuj));
  cue_details->set_suggestion_text(
      cue.anchored_message_cue().anchored_message_text());
  cue_details->set_promoted_feature(std::string(promoted_feature));
  cue_details->set_button_text(cue.anchored_message_cue().action_text());
  if (cue.has_gemini_in_chrome_surface()) {
    cue_details->set_prompt(cue.gemini_in_chrome_surface().prompt());
  } else if (cue.has_contextual_search_surface()) {
    cue_details->set_prompt(cue.contextual_search_surface().query());
  }
}

}  // namespace private_insights
