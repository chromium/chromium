// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_HATS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_HATS_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// Settings page UI handler that shows HaTS surveys.
class HatsHandler : public SettingsPageUIHandler {
 public:
  HatsHandler();

  // Not copyable or movable
  HatsHandler(const HatsHandler&) = delete;
  HatsHandler& operator=(const HatsHandler&) = delete;

  ~HatsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  void HandleTrustSafetyInteractionOccurred(const base::Value::List& args);

  void HandleSecurityPageHatsRequest(const base::Value::List& args);

 private:
  friend class HatsHandlerTest;
  friend class HatsHandlerParamTest;
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerTest, PrivacySettingsHats);
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerTest, PrivacyGuideHats);
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerTest, PrivacySandboxHats);
  FRIEND_TEST_ALL_PREFIXES(
      HatsHandlerTest,
      HandleSecurityPageHatsRequestPassesArgumentsToHatsService);
  FRIEND_TEST_ALL_PREFIXES(
      HatsHandlerTest,
      HandleSecurityPageHatsRequestPassesArgumentsToHatsServiceNotLaunchSurveyNotEnoughTime);
  FRIEND_TEST_ALL_PREFIXES(
      HatsHandlerTest,
      HandleSecurityPageHatsRequestPassesArgumentsToHatsServiceNotLaunchSurveyNoInteraction);
  FRIEND_TEST_ALL_PREFIXES(
      HatsHandlerTest,
      HandleSecurityPageHatsRequestPassesFriendlierSafeBrowsingSettingsStateToHatsService);
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerTest, TrustSafetySentimentInteractions);
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerNoSandboxTest, PrivacySettings);
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerNoSandboxTest,
                           TrustSafetySentimentInteractions);
  FRIEND_TEST_ALL_PREFIXES(HatsHandlerParamTest, AdPrivacyHats);

  // All Trust & Safety based interactions which may result in a HaTS survey.
  // Must be kept in sync with the enum of the same name in
  // hats_browser_proxy.js
  enum class TrustSafetyInteraction {
    RAN_SAFETY_CHECK = 0,
    USED_PRIVACY_CARD = 1,
    // OPENED_PRIVACY_SANDBOX = 2, // DEPRECATED
    OPENED_PASSWORD_MANAGER = 3,
    COMPLETED_PRIVACY_GUIDE = 4,
    RAN_PASSWORD_CHECK = 5,
    OPENED_AD_PRIVACY = 6,
    OPENED_TOPICS_SUBPAGE = 7,
    OPENED_FLEDGE_SUBPAGE = 8,
    OPENED_AD_MEASUREMENT_SUBPAGE = 9,
    // OPENED_GET_MOST_CHROME = 10, // DEPRECATED
  };

  /**
   * All interactions from the security settings page which may result in a HaTS
   * survey. Must be kept in sync with the enum of the same name in
   * hats_browser_proxy.js
   */
  enum class SecurityPageInteraction {
    RADIO_BUTTON_ENHANCED_CLICK = 0,
    RADIO_BUTTON_STANDARD_CLICK = 1,
    RADIO_BUTTON_DISABLE_CLICK = 2,
    EXPAND_BUTTON_ENHANCED_CLICK = 3,
    EXPAND_BUTTON_STANDARD_CLICK = 4,
    NO_INTERACTION = 5,
  };

  /**
   * Enumeration of all safe browsing modes. Must be kept in sync with the enum
   * of the same name located in:
   * chrome/browser/safe_browsing/generated_safe_browsing_pref.h
   */
  enum class SafeBrowsingSetting {
    ENHANCED = 0,
    STANDARD = 1,
    DISABLED = 2,
  };

  // Requests the appropriate HaTS survey, which may be none, for |interaction|.
  void RequestHatsSurvey(TrustSafetyInteraction interaction);

  // Informs the sentiment service, if appropriate, that |interaction| occurred.
  void InformSentimentService(TrustSafetyInteraction interaction);

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

  /**
   * Generate the Product Specific string data from |profile| and |args| for
   * chrome://settings/security page HaTS.
   * - First arg in the list indicates the SecurityPageInteraction.
   * - Second arg in the list indicates the SafeBrowsingSetting.
   */
  SurveyStringData GetSecurityPageProductSpecificStringData(
      Profile* profile,
      const base::Value::List& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_HATS_HANDLER_H_
