// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/ui/hats/hats_survey_status_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

class Browser;
class PrefRegistrySimple;
class Profile;

// Trigger identifiers currently used; duplicates not allowed.
extern const char kHatsSurveyTriggerTesting[];
extern const char kHatsSurveyTriggerSatisfaction[];
extern const char kHatsSurveyTriggerSettings[];
extern const char kHatsSurveyTriggerSettingsPrivacy[];

// The Trigger ID for a test HaTS Next survey which is available for testing
// and demo purposes when the migration feature flag is enabled.
extern const char kHatsNextSurveyTriggerIDTesting[];

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsService : public KeyedService {
 public:
  struct SurveyConfig {
    SurveyConfig(const double probability, const std::string en_site_id)
        : probability_(probability), en_site_id_(en_site_id) {}

    SurveyConfig() = default;

    // Probability [0,1] of how likely a chosen user will see the survey.
    double probability_;

    // Site ID for the survey.
    std::string en_site_id_;
  };

  struct SurveyMetadata {
    SurveyMetadata();
    ~SurveyMetadata();

    base::Optional<int> last_major_version;
    base::Optional<base::Time> last_survey_started_time;
    base::Optional<bool> is_survey_full;
    base::Optional<base::Time> last_survey_check_time;
  };

  class DelayedSurveyTask : public content::WebContentsObserver {
   public:
    DelayedSurveyTask(HatsService* hats_service,
                      const std::string& trigger,
                      content::WebContents* web_contents);

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
    base::WeakPtrFactory<DelayedSurveyTask> weak_ptr_factory_{this};
  };

  ~HatsService() override;

  explicit HatsService(Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Launches survey with identifier |trigger| if appropriate.
  virtual void LaunchSurvey(const std::string& trigger);

  // Launches survey (with id |trigger|) with a timeout |timeout_ms| if
  // appropriate. Survey will be shown at the active window/tab by the
  // time of launching. Rejects (and returns false) if the underlying task
  // posting fails.
  virtual bool LaunchDelayedSurvey(const std::string& trigger, int timeout_ms);

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
      int timeout_ms);

  // Updates the user preferences to record that the survey associated with
  // |survey_id| was shown to the user. |survey_id| is the unique_id provided
  // to the HaTS Service to identify a survey. This is the trigger ID for HaTS
  // Next, and the site ID for HaTS v1.
  void RecordSurveyAsShown(std::string survey_id);

  // Indicates to the service that the HaTS Next dialog has been closed.
  // Virtual to allow mocking in tests.
  virtual void HatsNextDialogClosed();

  void SetSurveyMetadataForTesting(const SurveyMetadata& metadata);
  void GetSurveyMetadataForTesting(HatsService::SurveyMetadata* metadata) const;
  void SetSurveyCheckerForTesting(
      std::unique_ptr<HatsSurveyStatusChecker> checker);
  bool HasPendingTasks();

 private:
  friend class DelayedSurveyTask;
  FRIEND_TEST_ALL_PREFIXES(HatsServiceHatsNext, SingleHatsNextDialog);

  void LaunchSurveyForWebContents(const std::string& trigger,
                                  content::WebContents* web_contents);

  void LaunchSurveyForBrowser(const std::string& trigger, Browser* browser);

  // Returns true is the survey trigger specified should be shown.
  bool ShouldShowSurvey(const std::string& trigger) const;

  // Check whether the survey is reachable and under capacity.
  void CheckSurveyStatusAndMaybeShow(Browser* browser,
                                     const std::string& trigger);

  // Callbacks for survey capacity checking.
  void ShowSurvey(Browser* browser, const std::string& trigger);

  void OnSurveyStatusError(const std::string& trigger,
                           HatsSurveyStatusChecker::Status error);
  void RemoveTask(const DelayedSurveyTask& task);

  // Profile associated with this service.
  Profile* const profile_;

  std::unique_ptr<HatsSurveyStatusChecker> checker_;

  std::set<DelayedSurveyTask> pending_tasks_;

  base::flat_map<std::string, SurveyConfig> survey_configs_by_triggers_;

  // Whether a HaTS Next dialog currently exists (regardless of whether it
  // is being shown to the user).
  bool hats_next_dialog_exists_ = false;

  base::WeakPtrFactory<HatsService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HatsService);
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_H_
