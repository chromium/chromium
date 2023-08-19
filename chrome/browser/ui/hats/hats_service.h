// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class Browser;
class Profile;

// The name of the histogram which records if a survey was shown, or if not, the
// reason why not.
extern const char kHatsShouldShowSurveyReasonHistogram[];

// Key-value mapping type for survey's product specific bits data.
typedef std::map<std::string, bool> SurveyBitsData;

// Key-value mapping type for survey's product specific string data.
typedef std::map<std::string, std::string> SurveyStringData;

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsService : public KeyedService {
 public:
  struct SurveyMetadata {
    SurveyMetadata();
    ~SurveyMetadata();

    // Trigger specific metadata.
    absl::optional<int> last_major_version;
    absl::optional<base::Time> last_survey_started_time;
    absl::optional<bool> is_survey_full;
    absl::optional<base::Time> last_survey_check_time;

    // Metadata affecting all triggers.
    absl::optional<base::Time> any_last_survey_started_time;
  };

  class DelayedSurveyTask : public content::WebContentsObserver {
   public:
    DelayedSurveyTask(HatsService* hats_service,
                      const std::string& trigger,
                      content::WebContents* web_contents,
                      const SurveyBitsData& product_specific_bits_data,
                      const SurveyStringData& product_specific_string_data,
                      bool require_same_origin);

    // Not copyable or movable
    DelayedSurveyTask(const DelayedSurveyTask&) = delete;
    DelayedSurveyTask& operator=(const DelayedSurveyTask&) = delete;

    ~DelayedSurveyTask() override;

    // Asks |hats_service_| to launch the survey with id |trigger_| for tab
    // |web_contents_|.
    void Launch();

    // content::WebContentsObserver
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;
    void WebContentsDestroyed() override;

    // Returns a weak pointer to this object.
    base::WeakPtr<DelayedSurveyTask> GetWeakPtr();

    bool operator<(const HatsService::DelayedSurveyTask& other) const {
      return trigger_ < other.trigger_ ? true
                                       : web_contents() < other.web_contents();
    }

   private:
    raw_ptr<HatsService> hats_service_;
    std::string trigger_;
    SurveyBitsData product_specific_bits_data_;
    SurveyStringData product_specific_string_data_;
    bool require_same_origin_;
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

  explicit HatsService(Profile* profile);

  HatsService(const HatsService&) = delete;
  HatsService& operator=(const HatsService&) = delete;

  ~HatsService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Launches survey with identifier |trigger| if appropriate.
  // |success_callback| is called when the survey is shown to the user.
  // |failure_callback| is called if the survey does not launch for any reason.
  // |product_specific_bits_data| and |product_specific_string_data| must
  // contain key-value pairs where the keys match the field names set for the
  // survey in hats_service.cc, and the values are those which will be
  // associated with the survey response. Field's matches are CHECK enforced.
  virtual void LaunchSurvey(
      const std::string& trigger,
      base::OnceClosure success_callback = base::DoNothing(),
      base::OnceClosure failure_callback = base::DoNothing(),
      const SurveyBitsData& product_specific_bits_data = {},
      const SurveyStringData& product_specific_string_data = {});

  // Launches survey (with id |trigger|) with a timeout |timeout_ms| if
  // appropriate. Survey will be shown at the active window/tab by the
  // time of launching. Rejects (and returns false) if the underlying task
  // posting fails.
  virtual bool LaunchDelayedSurvey(
      const std::string& trigger,
      int timeout_ms,
      const SurveyBitsData& product_specific_bits_data = {},
      const SurveyStringData& product_specific_string_data = {});

  // Launches survey (with id |trigger|) with a timeout |timeout_ms| for tab
  // |web_contents| if appropriate. |web_contents| required to be non-nullptr.
  // Launch is cancelled if |web_contents| killed before end of timeout. Launch
  // is also cancelled if |web_contents| not visible at the time of launch.
  // Rejects (and returns false) if there is already an identical delayed-task
  // (same |trigger| and same |web_contents|) waiting to be fulfilled. Also
  // rejects if the underlying task posting fails. If |require_same_origin| is
  // set, additionally requires that |web_contents| remain on the same origin.
  virtual bool LaunchDelayedSurveyForWebContents(
      const std::string& trigger,
      content::WebContents* web_contents,
      int timeout_ms,
      const SurveyBitsData& product_specific_bits_data = {},
      const SurveyStringData& product_specific_string_data = {},
      bool require_same_origin = false);

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

  // Whether the user is eligible for any survey (of the type |user_prompted|
  // or not) to be shown. A return value of false is always a true-negative, and
  // means the user is currently ineligible for all surveys. A return value of
  // true should not be interpreted as a guarantee that requests to show a
  // survey will succeed. Virtual to allow mocking in tests.
  virtual bool CanShowAnySurvey(bool user_prompted) const;

  // Returns whether a HaTS Next dialog currently exists, regardless of whether
  // it is being shown or not.
  bool hats_next_dialog_exists_for_testing() {
    return hats_next_dialog_exists_;
  }

 private:
  friend class DelayedSurveyTask;
  FRIEND_TEST_ALL_PREFIXES(HatsServiceProbabilityOne, SingleHatsNextDialog);

  using SurveyConfigs = base::flat_map<std::string, hats::SurveyConfig>;

  void LaunchSurveyForWebContents(
      const std::string& trigger,
      content::WebContents* web_contents,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data);

  void LaunchSurveyForBrowser(
      Browser* browser,
      const std::string& trigger,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data);

  // Returns true is the survey trigger specified should be shown.
  bool ShouldShowSurvey(const std::string& trigger) const;

  // Check whether the survey is reachable and under capacity and show it.
  // |success_callback| is called when the survey is shown to the user.
  // |failure_callback| is called if the survey does not launch for any reason.
  // The matches of field names with the `SurveyConfig` are CHECK enforced.
  void CheckSurveyStatusAndMaybeShow(
      Browser* browser,
      const std::string& trigger,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data);

  // Remove |task| from the set of |pending_tasks_|.
  void RemoveTask(const DelayedSurveyTask& task);

  // Profile associated with this service.
  const raw_ptr<Profile> profile_;

  std::set<DelayedSurveyTask> pending_tasks_;

  SurveyConfigs survey_configs_by_triggers_;

  // Whether a HaTS Next dialog currently exists (regardless of whether it
  // is being shown to the user).
  bool hats_next_dialog_exists_ = false;

  base::WeakPtrFactory<HatsService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
