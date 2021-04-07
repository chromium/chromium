// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class Browser;
class Profile;

// Trigger identifiers currently used; duplicates not allowed.
extern const char kHatsSurveyTriggerTesting[];
extern const char kHatsSurveyTriggerPrivacySandbox[];
extern const char kHatsSurveyTriggerSettings[];
extern const char kHatsSurveyTriggerSettingsPrivacy[];
extern const char kHatsSurveyTriggerNtpModules[];
extern const char kHatsSurveyTriggerDevToolsIssuesCOEP[];
extern const char kHatsSurveyTriggerDevToolsIssuesMixedContent[];
extern const char kHatsSurveyTriggerDevToolsIssuesCookiesSameSite[];
extern const char kHatsSurveyTriggerDevToolsIssuesHeavyAd[];
extern const char kHatsSurveyTriggerDevToolsIssuesCSP[];

// The Trigger ID for a test HaTS Next survey which is available for testing
// and demo purposes when the migration feature flag is enabled.
extern const char kHatsNextSurveyTriggerIDTesting[];

// The name of the histogram which records if a survey was shown, or if not, the
// reason why not.
extern const char kHatsShouldShowSurveyReasonHistogram[];

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsService : public KeyedService {
 public:
  struct SurveyConfig {
    // Constructs a SurveyConfig by inspecting |feature|. This includes checking
    // if the feature is enabled, as well as inspecting the feature parameters
    // for the survey probability, and if |presupplied_trigger_id| is not
    // provided, the trigger ID.
    SurveyConfig(
        const base::Feature* feature,
        const std::string& trigger,
        const base::Optional<std::string>& presupplied_trigger_id =
            base::nullopt,
        const std::vector<std::string>& product_specific_data_fields = {});
    SurveyConfig();
    SurveyConfig(const SurveyConfig&);
    ~SurveyConfig();

    // Whether the survey is currently enabled and can be shown.
    bool enabled = false;

    // Probability [0,1] of how likely a chosen user will see the survey.
    double probability = 0.0f;

    // The trigger for this survey within the browser.
    std::string trigger;

    // Trigger ID for the survey.
    std::string trigger_id;

    // The survey will prompt every time because the user has explicitly decided
    // to take the survey e.g. clicking a link.
    bool user_prompted = false;

    // Product Specific Data fields which are sent with the survey
    // response.
    std::vector<std::string> product_specific_data_fields;
  };

  struct SurveyMetadata {
    SurveyMetadata();
    ~SurveyMetadata();

    // Trigger specific metadata.
    base::Optional<int> last_major_version;
    base::Optional<base::Time> last_survey_started_time;
    base::Optional<bool> is_survey_full;
    base::Optional<base::Time> last_survey_check_time;

    // Metadata affecting all triggers.
    base::Optional<base::Time> any_last_survey_started_time;
  };

  class DelayedSurveyTask : public content::WebContentsObserver {
   public:
    DelayedSurveyTask(HatsService* hats_service,
                      const std::string& trigger,
                      content::WebContents* web_contents,
                      const std::map<std::string, bool>& product_specific_data);

    // Not copyable or movable
    DelayedSurveyTask(const DelayedSurveyTask&) = delete;
    DelayedSurveyTask& operator=(const DelayedSurveyTask&) = delete;

    ~DelayedSurveyTask() override;

    // Asks |hats_service_| to launch the survey with id |trigger_| for tab
    // |web_contents_|.
    void Launch();

    // content::WebContentsObserver
    void WebContentsDestroyed() override;

    // Returns a weak pointer to this object.
    base::WeakPtr<DelayedSurveyTask> GetWeakPtr();

    bool operator<(const HatsService::DelayedSurveyTask& other) const {
      return trigger_ < other.trigger_ ? true
                                       : web_contents() < other.web_contents();
    }

   private:
    HatsService* hats_service_;
    std::string trigger_;
    std::map<std::string, bool> product_specific_data_;
    base::WeakPtrFactory<DelayedSurveyTask> weak_ptr_factory_{this};
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ShouldShowSurveyReasons {
    kYes = 0,
    kNoOffline = 1,
    kNoLastSessionCrashed = 2,
    kNoReceivedSurveyInCurrentMilestone = 3,
    kNoProfileTooNew = 4,
    kNoLastSurveyTooRecent = 5,
    kNoBelowProbabilityLimit = 6,
    kNoTriggerStringMismatch = 7,
    kNoNotRegularBrowser = 8,
    kNoIncognitoDisabled = 9,
    kNoCookiesBlocked = 10,            // Unused.
    kNoThirdPartyCookiesBlocked = 11,  // Unused.
    kNoSurveyUnreachable = 12,
    kNoSurveyOverCapacity = 13,
    kNoSurveyAlreadyInProgress = 14,
    kNoAnyLastSurveyTooRecent = 15,
    kNoRejectedByHatsService = 16,
    kMaxValue = kNoRejectedByHatsService,
  };

  ~HatsService() override;

  explicit HatsService(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Launches survey with identifier |trigger| if appropriate.
  // |success_callback| is called when the survey is shown to the user.
  // |failure_callback| is called if the survey does not launch for any reason.
  // |product_specific_data| should contain key-value pairs where the keys match
  // the field names set for the survey in hats_service.cc, and the values are
  // those which will be associated with the survey response.
  virtual void LaunchSurvey(
      const std::string& trigger,
      base::OnceClosure success_callback = base::DoNothing(),
      base::OnceClosure failure_callback = base::DoNothing(),
      const std::map<std::string, bool>& product_specific_data = {});

  // Launches survey (with id |trigger|) with a timeout |timeout_ms| if
  // appropriate. Survey will be shown at the active window/tab by the
  // time of launching. Rejects (and returns false) if the underlying task
  // posting fails.
  virtual bool LaunchDelayedSurvey(
      const std::string& trigger,
      int timeout_ms,
      const std::map<std::string, bool>& product_specific_data = {});

  // Launches survey (with id |trigger|) with a timeout |timeout_ms| for tab
  // |web_contents| if appropriate. |web_contents| required to be non-nullptr.
  // Launch is cancelled if |web_contents| killed before end of timeout. Launch
  // is also cancelled if |web_contents| not visible at the time of launch.
  // Rejects (and returns false) if there is already an identical delayed-task
  // (same |trigger| and same |web_contents|) waiting to be fulfilled. Also
  // rejects if the underlying task posting fails.
  virtual bool LaunchDelayedSurveyForWebContents(
      const std::string& trigger,
      content::WebContents* web_contents,
      int timeout_ms,
      const std::map<std::string, bool>& product_specific_data = {});

  // Updates the user preferences to record that the survey associated with
  // |survey_id| was shown to the user. |trigger_id| is the HaTS next Trigger
  // ID for the survey.
  void RecordSurveyAsShown(std::string trigger_id);

  // Indicates to the service that the HaTS Next dialog has been closed.
  // Virtual to allow mocking in tests.
  virtual void HatsNextDialogClosed();

  void SetSurveyMetadataForTesting(const SurveyMetadata& metadata);
  void GetSurveyMetadataForTesting(HatsService::SurveyMetadata* metadata) const;
  bool HasPendingTasks();

  // Whether the survey specified by |trigger| can be shown to the user. This
  // is a pre-check that calculates as many conditions as possible, but could
  // still return a false positive due to client-side rate limiting, a change
  // in network conditions, or intervening calls to this API.
  bool CanShowSurvey(const std::string& trigger) const;

  // Returns whether a HaTS Next dialog currently exists, regardless of whether
  // it is being shown or not.
  bool hats_next_dialog_exists_for_testing() {
    return hats_next_dialog_exists_;
  }

 private:
  friend class DelayedSurveyTask;
  FRIEND_TEST_ALL_PREFIXES(HatsServiceProbabilityOne, SingleHatsNextDialog);

  void LaunchSurveyForWebContents(
      const std::string& trigger,
      content::WebContents* web_contents,
      const std::map<std::string, bool>& product_specific_data);

  void LaunchSurveyForBrowser(
      Browser* browser,
      const std::string& trigger,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const std::map<std::string, bool>& product_specific_data);

  // Returns true is the survey trigger specified should be shown.
  bool ShouldShowSurvey(const std::string& trigger) const;

  // Check whether the survey is reachable and under capacity and show it.
  // |success_callback| is called when the survey is shown to the user.
  // |failure_callback| is called if the survey does not launch for any reason.
  void CheckSurveyStatusAndMaybeShow(
      Browser* browser,
      const std::string& trigger,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const std::map<std::string, bool>& product_specific_data);

  // Remove |task| from the set of |pending_tasks_|.
  void RemoveTask(const DelayedSurveyTask& task);

  // Profile associated with this service.
  Profile* const profile_;

  std::set<DelayedSurveyTask> pending_tasks_;

  base::flat_map<std::string, SurveyConfig> survey_configs_by_triggers_;

  // Whether a HaTS Next dialog currently exists (regardless of whether it
  // is being shown to the user).
  bool hats_next_dialog_exists_ = false;

  base::WeakPtrFactory<HatsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HatsService);
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
