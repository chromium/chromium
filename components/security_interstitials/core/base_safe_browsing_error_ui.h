// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_BASE_SAFE_BROWSING_ERROR_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_BASE_SAFE_BROWSING_ERROR_UI_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

namespace security_interstitials {

struct InterstitialInteractionDetails {
  InterstitialInteractionDetails(int occurrence_count,
                                 int64_t first_timestamp,
                                 int64_t last_timestamp);
  int occurrence_count;
  int64_t first_timestamp;
  int64_t last_timestamp;
};

using InterstitialInteractionMap =
    std::map<SecurityInterstitialCommand, InterstitialInteractionDetails>;

// A base class for quiet vs loud versions of the safe browsing interstitial.
// This class displays UI for Safe Browsing errors that block page loads. This
// class is purely about visual display; it does not do any error-handling logic
// to determine what type of error should be displayed when.
class BaseSafeBrowsingErrorUI {
 public:
  enum SBInterstitialReason {
    SB_REASON_MALWARE,
    SB_REASON_HARMFUL,
    SB_REASON_PHISHING,
    SB_REASON_BILLING,
  };

  struct SBErrorDisplayOptions {
    SBErrorDisplayOptions(bool is_main_frame_load_pending,
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
                          const std::string& help_center_article_link);

    SBErrorDisplayOptions(const SBErrorDisplayOptions& other);

    // Indicates if this SB interstitial is shown when the main frame load is
    // pending.
    bool is_main_frame_load_pending;

    // Indicates if user is allowed to opt-in extended reporting preference.
    bool is_extended_reporting_opt_in_allowed;

    // Indicates if user is in incognito mode.
    bool is_off_the_record;

    // Indicates if user opted in for SB extended reporting.
    bool is_extended_reporting_enabled;

    // Whether the SBER pref is being managed by enterprise policy, meaning the
    // user is unable to change the pref.
    bool is_extended_reporting_policy_managed;

    // Indicates if enhanced protection is on for the user.
    bool is_enhanced_protection_enabled;

    // Indicates if kSafeBrowsingProceedAnywayDisabled preference is set.
    bool is_proceed_anyway_disabled;

    // Indicates if links should use a new foreground tab or the current tab.
    bool should_open_links_in_new_tab;

    // Indicates if the 'Back to safety' primary action button should always be
    // shown. If the option is false, this button is shown only when there is
    // a proper page to navigate back to. Chrome and Chromium builds should
    // always set this option to true,
    bool always_show_back_to_safety;

    // Indicates if the feature to show enhanced protection message on the
    // interstitial is enabled.
    bool is_enhanced_protection_message_enabled;

    // Indicates if Safe Browsing is managed.
    bool is_safe_browsing_managed;

    // The p= query parameter used when visiting the Help Center. If this is
    // nullptr, then a default value will be used for the SafeBrowsing article.
    std::string help_center_article_link;
  };

  BaseSafeBrowsingErrorUI(
      const GURL& request_url,
      BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      const std::string& app_locale,
      const base::Time& time_triggered,
      ControllerClient* controller);

  BaseSafeBrowsingErrorUI(const BaseSafeBrowsingErrorUI&) = delete;
  BaseSafeBrowsingErrorUI& operator=(const BaseSafeBrowsingErrorUI&) = delete;

  virtual ~BaseSafeBrowsingErrorUI();

  bool is_main_frame_load_pending() const {
    return display_options_.is_main_frame_load_pending;
  }

  bool is_extended_reporting_opt_in_allowed() const {
    return display_options_.is_extended_reporting_opt_in_allowed;
  }

  bool is_off_the_record() const { return display_options_.is_off_the_record; }

  bool is_extended_reporting_enabled() const {
    return display_options_.is_extended_reporting_enabled;
  }

  void set_extended_reporting(bool pref) {
    display_options_.is_extended_reporting_enabled = pref;
  }

  bool is_extended_reporting_policy_managed() const {
    return display_options_.is_extended_reporting_policy_managed;
  }

  bool is_enhanced_protection_enabled() const {
    return display_options_.is_enhanced_protection_enabled;
  }

  bool is_proceed_anyway_disabled() const {
    return display_options_.is_proceed_anyway_disabled;
  }

  bool should_open_links_in_new_tab() const {
    return display_options_.should_open_links_in_new_tab;
  }

  bool always_show_back_to_safety() const {
    return display_options_.always_show_back_to_safety;
  }

  const std::string& get_help_center_article_link() const {
    return display_options_.help_center_article_link;
  }

  bool is_enhanced_protection_message_enabled() const {
    return display_options_.is_enhanced_protection_message_enabled;
  }

  bool is_safe_browsing_managed() const {
    return display_options_.is_safe_browsing_managed;
  }

  const SBErrorDisplayOptions& get_error_display_options() const {
    return display_options_;
  }

  // Checks if we should even show the extended reporting option.
  // We don't show it:
  // - in incognito mode
  // - if kSafeBrowsingExtendedReportingOptInAllowed preference is disabled.
  // - if kSafeBrowsingExtendedReporting is managed by enterprise policy.
  // - if enhanced protection is on
  bool CanShowExtendedReportingOption() {
    return !is_off_the_record() && is_extended_reporting_opt_in_allowed() &&
           !is_extended_reporting_policy_managed() &&
           !is_enhanced_protection_enabled();
  }

  // Checks if we should even show the enhanced protection message.
  // We don't show it:
  // - in incognito mode, OR
  // - if kEnhancedProtectionMessageInInterstitials flag is disabled, OR
  // - if kSafeBrowsingEnabled or kSafeBrowsingEnhanced is managed by enterprise
  // policy, OR
  // - if enhanced protection is on
  bool CanShowEnhancedProtectionMessage() {
    return !is_off_the_record() && is_enhanced_protection_message_enabled() &&
           !is_safe_browsing_managed() && !is_enhanced_protection_enabled();
  }

  SBInterstitialReason interstitial_reason() const {
    return interstitial_reason_;
  }

  const std::string app_locale() const { return app_locale_; }

  ControllerClient* controller() { return controller_; }

  GURL request_url() const { return request_url_; }

  bool did_user_make_decision() { return user_made_decision_; }

  std::unique_ptr<InterstitialInteractionMap>
  get_interstitial_interaction_data() {
    return std::move(interstitial_interaction_data_);
  }

  virtual void PopulateStringsForHtml(base::Value::Dict& load_time_data) = 0;
  virtual void HandleCommand(SecurityInterstitialCommand command) = 0;

  virtual int GetHTMLTemplateId() const = 0;

 protected:
  // Records the number of occurrences of different user interactions with a
  // security interstitial. Used for metrics.
  std::unique_ptr<InterstitialInteractionMap> interstitial_interaction_data_;
  bool user_made_decision_;

 private:
  const GURL request_url_;
  const SBInterstitialReason interstitial_reason_;
  SBErrorDisplayOptions display_options_;
  const std::string app_locale_;
  const base::Time time_triggered_;

  raw_ptr<ControllerClient> controller_;
};

}  // security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_BASE_SAFE_BROWSING_ERROR_UI_H_
