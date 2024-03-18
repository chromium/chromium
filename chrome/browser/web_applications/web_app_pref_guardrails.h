// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREF_GUARDRAILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREF_GUARDRAILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/common/web_app_id.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

struct GuardrailData {
  std::optional<int> app_specific_not_accept_count;
  std::optional<int> app_specific_mute_after_dismiss_days;
  std::optional<int> app_specific_mute_after_ignore_days;
  int global_not_accept_count;
  std::optional<int> global_mute_after_dismiss_days;
  std::optional<int> global_mute_after_ignore_days;
};

struct GuardrailPrefNames {
  std::string_view last_ignore_time_name;
  std::string_view last_dismiss_time_name;
  std::string_view not_accepted_count_name;
  std::string_view all_blocked_time_name;
  std::string_view global_pref_name;
  std::string_view block_reason_name;
};

std::optional<int> GetIntWebAppPref(const PrefService* pref_service,
                                    const webapps::AppId& app_id,
                                    std::string_view path);

std::optional<base::Time> GetTimeWebAppPref(const PrefService* pref_service,
                                            const webapps::AppId& app_id,
                                            std::string_view path);

// WebAppPrefGuardrails provide a simple way of building guardrails based on the
// number of times a prompt on an app has been ignored or dismissed in the past.
// The guardrails help prevent the prompt from showing up after a specific
// number of times based on the user behavior. Data for computing these
// guardrails are stored in the prefs.
class WebAppPrefGuardrails {
 public:
  // Returns an instance of the WebAppPrefGuardrails built to handle when the
  // IPH bubble for the desktop install prompt should be shown.
  static WebAppPrefGuardrails GetForDesktopInstallIph(
      PrefService* pref_service);

  // Returns an instance of the WebAppPrefGuardrails built to handle when the
  // ML triggered install prompt should be shown for web apps.
  static WebAppPrefGuardrails GetForMlInstallPrompt(PrefService* pref_service);

  // Returns an instance of the WebAppPrefGuardrails built to handle when the
  // IPH bubble for apps launched via link capturing should be shown.
  static WebAppPrefGuardrails GetForLinkCapturingIph(PrefService* pref_service);

  // The time values are stored as a string-flavored base::value representing
  // the int64_t number of microseconds since the Windows epoch, using
  // base::TimeToValue(). The stored preferences look like:
  //   "web_app_ids": {
  //     "<app_id_1>": {
  //       "was_external_app_uninstalled_by_user": true,
  //       "IPH_num_of_consecutive_ignore": 2,
  //       "IPH_link_capturing_consecutive_not_accepted_num": 2,
  //       "ML_num_of_consecutive_not_accepted": 2,
  //       "IPH_last_ignore_time": "13249617864945580",
  //       "ML_last_time_install_ignored": "13249617864945580",
  //       "ML_last_time_install_dismissed": "13249617864945580",
  //       "IPH_link_capturing_last_time_ignored": "13249617864945580",
  //       "error_loaded_policy_app_migrated": true
  //     },
  //   },
  //   "app_agnostic_ml_state": {
  //       "ML_last_time_install_ignored": "13249617864945580",
  //       "ML_last_time_install_dismissed": "13249617864945580",
  //       "ML_num_of_consecutive_not_accepted": 2,
  //       "ML_all_promos_blocked_date": "13249617864945580",
  //   },
  //   "app_agnostic_iph_state": {
  //     "IPH_num_of_consecutive_ignore": 3,
  //     "IPH_last_ignore_time": "13249617864945500",
  //   },
  //   "app_agnostic_iph_link_capturing_state": {
  //     "IPH_link_capturing_consecutive_not_accepted_num": 3,
  //     "IPH_link_capturing_last_time_ignored": "13249617864945500",
  //     "IPH_link_capturing_blocked_date": "13249617864945500",
  //     "IPH_link_capturing_block_reason":
  //     "app_specific_ignore_count_hit:app_id"
  //   }
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  ~WebAppPrefGuardrails();
  WebAppPrefGuardrails(const WebAppPrefGuardrails& other) = delete;
  WebAppPrefGuardrails& operator=(const WebAppPrefGuardrails& other) = delete;

  // Record that the prompt on app corresponding to app_id being ignored at a
  // specific time.
  void RecordIgnore(const webapps::AppId& app_id, base::Time time);
  // Record that the prompt on app corresponding to app_id being dismissed at a
  // specific time.
  void RecordDismiss(const webapps::AppId& app_id, base::Time time);
  // Record that the prompt on app corresponding to app_id is accepted.
  void RecordAccept(const webapps::AppId& app_id);
  // Returns whether a new prompt should be shown or not for app_id based on
  // values stored in the pref_names_.
  bool IsBlockedByGuardrails(const webapps::AppId& app_id);

 private:
  WebAppPrefGuardrails(PrefService* profile,
                       const GuardrailData& guardrail_data,
                       const GuardrailPrefNames& guardrail_pref_names,
                       std::optional<int> max_days_to_store_guardrails);

  // If guardrails are blocked, returns a string result of why it was blocked.
  std::optional<std::string> IsAppBlocked(const webapps::AppId& app_id);
  std::optional<std::string> IsGloballyBlocked();

  void UpdateAppSpecificNotAcceptedPrefs(const webapps::AppId& app_id,
                                         base::Time time,
                                         std::string_view time_path);
  void UpdateGlobalNotAcceptedPrefs(base::Time time,
                                    std::string_view time_path);

  // If a prompt is already blocked by guardrails, return whether that should be
  // reset.
  bool ShouldResetGlobalGuardrails();
  void ResetGlobalGuardrails(const webapps::AppId& app_id);

  bool IsGlobalBlockActive();
  void LogGlobalBlockReason(ScopedDictPrefUpdate& global_update,
                            const std::string& reason);

  // Pref update functions.
  void UpdateTimeWebAppPref(const webapps::AppId& app_id,
                            std::string_view path,
                            base::Time value);

  void UpdateIntWebAppPref(const webapps::AppId& app_id,
                           std::string_view path,
                           int value);

  raw_ptr<PrefService> pref_service_;
  const raw_ref<const GuardrailData> guardrail_data_;
  const raw_ref<const GuardrailPrefNames> pref_names_;

  // This cannot be a part of the GuardrailData struct since this is dynamic and
  // is usually controlled via Finch, and is hence not a constant. If not
  // defined or set to std::nullopt, guardrails will never be reset.
  std::optional<int> max_days_to_store_guardrails_;
};

inline constexpr GuardrailData kIphGuardrails{
    // Number of times IPH can be ignored for this app before it's muted.
    .app_specific_not_accept_count = 3,
    // Number of days to mute IPH after it's ignored for this app.
    .app_specific_mute_after_ignore_days = 90,
    // Number of times IPH can be ignored for any app before it's muted.
    .global_not_accept_count = 4,
    // Number of days to mute IPH after it's ignored for any app.
    .global_mute_after_ignore_days = 14,
};

inline constexpr GuardrailPrefNames kIphPrefNames{
    // Pref key to store the last time IPH was ignored, stored in both app
    // specific and app agnostic context.
    .last_ignore_time_name = "IPH_last_ignore_time",
    // Pref key to store the total number of ignores on the IPH bubble, stored
    // in both app specific and app agnostic context.
    .not_accepted_count_name = "IPH_num_of_consecutive_ignore",
    // Pref key under which to store app agnostic IPH values.
    .global_pref_name = ::prefs::kWebAppsAppAgnosticIphState,
};

// ----------------------ML guardrails----------------------------
inline constexpr GuardrailData kMlPromoGuardrails{
    // Number of times ML triggered install dialog can be ignored for this app
    // before it's muted.
    .app_specific_not_accept_count = 3,
    // Number of days to mute install dialog for this app after the ML triggered
    // prompt was dismissed.
    .app_specific_mute_after_dismiss_days = 14,
    // Number of days to mute install dialog for this app after the ML triggered
    // prompt was ignored.
    .app_specific_mute_after_ignore_days = 2,
    // Number of times ML triggered install dialog can be ignored for all apps
    // before it's muted.
    .global_not_accept_count = 5,
    // Number of days to mute install dialog for any app after the ML triggered
    // prompt was dismissed.
    .global_mute_after_dismiss_days = 7,
    // Number of days to mute install dialog for any app after the ML triggered
    // prompt was ignored.
    .global_mute_after_ignore_days = 1,
};

inline constexpr GuardrailPrefNames kMlPromoPrefNames{
    .last_ignore_time_name = "ML_last_time_install_ignored",
    .last_dismiss_time_name = "ML_last_time_install_dismissed",
    .not_accepted_count_name = "ML_num_of_consecutive_not_accepted",
    .all_blocked_time_name = "ML_all_promos_blocked_date",
    .global_pref_name = ::prefs::kWebAppsAppAgnosticMlState,
    .block_reason_name = "ML_guardrail_blocked",
};

// -----------------------IPH Link Capturing guardrails-------------------
inline constexpr GuardrailData kIPHLinkCapturingGuardrails{
    // Number of times IPH bubble can show up for any apps launched via link
    // capturing before it's muted.
    .global_not_accept_count = 6,
    // Number of days to mute IPH for link captured app launches after it's
    // dismissed for any app.
    .global_mute_after_dismiss_days = 1,
};

inline constexpr GuardrailPrefNames kIPHLinkCapturingPrefNames{
    .last_dismiss_time_name = "IPH_link_capturing_last_time_dismissed",
    .not_accepted_count_name =
        "IPH_link_capturing_consecutive_not_accepted_num",
    .all_blocked_time_name = "IPH_link_capturing_blocked_date",
    .global_pref_name = ::prefs::kWebAppsAppAgnosticIPHLinkCapturingState,
    .block_reason_name = "IPH_link_capturing_block_reason",
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREF_GUARDRAILS_H_
