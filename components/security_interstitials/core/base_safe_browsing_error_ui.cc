// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"

namespace security_interstitials {

InterstitialInteractionDetails::InterstitialInteractionDetails(
    int occurrence_count,
    int64_t first_timestamp,
    int64_t last_timestamp)
    : occurrence_count(occurrence_count),
      first_timestamp(first_timestamp),
      last_timestamp(last_timestamp) {}

BaseSafeBrowsingErrorUI::BaseSafeBrowsingErrorUI(
    const GURL& request_url,
    BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    const std::string& app_locale,
    const base::Time& time_triggered,
    ControllerClient* controller)
    : request_url_(request_url),
      interstitial_reason_(reason),
      display_options_(display_options),
      app_locale_(app_locale),
      time_triggered_(time_triggered),
      controller_(controller) {}

BaseSafeBrowsingErrorUI::~BaseSafeBrowsingErrorUI() = default;

BaseSafeBrowsingErrorUI::SBErrorDisplayOptions::SBErrorDisplayOptions(
    bool is_main_frame_load_pending,
    bool is_extended_reporting_opt_in_allowed,
    bool is_off_the_record,
    bool is_extended_reporting_enabled,
    bool is_extended_reporting_policy_managed,
    bool is_enhanced_protection_enabled,
    bool is_proceed_anyway_disabled,
    bool should_open_links_in_new_tab,
    bool always_show_back_to_safety,
    bool is_enhanced_protection_message_enabled,
    bool is_safe_browsing_managed,
    const std::string& help_center_article_link)
    : is_main_frame_load_pending(is_main_frame_load_pending),
      is_extended_reporting_opt_in_allowed(
          is_extended_reporting_opt_in_allowed),
      is_off_the_record(is_off_the_record),
      is_extended_reporting_enabled(is_extended_reporting_enabled),
      is_extended_reporting_policy_managed(
          is_extended_reporting_policy_managed),
      is_enhanced_protection_enabled(is_enhanced_protection_enabled),
      is_proceed_anyway_disabled(is_proceed_anyway_disabled),
      should_open_links_in_new_tab(should_open_links_in_new_tab),
      always_show_back_to_safety(always_show_back_to_safety),
      is_enhanced_protection_message_enabled(
          is_enhanced_protection_message_enabled),
      is_safe_browsing_managed(is_safe_browsing_managed),
      help_center_article_link(help_center_article_link) {}

BaseSafeBrowsingErrorUI::SBErrorDisplayOptions::SBErrorDisplayOptions(
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& other)
    : is_main_frame_load_pending(other.is_main_frame_load_pending),
      is_extended_reporting_opt_in_allowed(
          other.is_extended_reporting_opt_in_allowed),
      is_off_the_record(other.is_off_the_record),
      is_extended_reporting_enabled(other.is_extended_reporting_enabled),
      is_extended_reporting_policy_managed(
          other.is_extended_reporting_policy_managed),
      is_enhanced_protection_enabled(other.is_enhanced_protection_enabled),
      is_proceed_anyway_disabled(other.is_proceed_anyway_disabled),
      should_open_links_in_new_tab(other.should_open_links_in_new_tab),
      always_show_back_to_safety(other.always_show_back_to_safety),
      is_enhanced_protection_message_enabled(
          other.is_enhanced_protection_message_enabled),
      is_safe_browsing_managed(other.is_safe_browsing_managed),
      help_center_article_link(other.help_center_article_link) {}

}  // security_interstitials
