// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_HATS_SERVICE_DESKTOP_H_
#define CHROME_BROWSER_UI_HATS_HATS_SERVICE_DESKTOP_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;

// Key-value mapping type for survey's product specific bits data.
typedef std::map<std::string, bool> SurveyBitsData;

// Key-value mapping type for survey's product specific string data.
typedef std::map<std::string, std::string> SurveyStringData;

// The name of the histogram which records if a survey was shown, or if not, the
// reason why not.
extern const char kHatsShouldShowSurveyReasonHistogram[];

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsServiceDesktop : public HatsService {
 public:
  class DelayedSurveyTask : public content::WebContentsObserver {
   public:
    DelayedSurveyTask(HatsServiceDesktop* hats_service,
                      std::string trigger,
                      content::WebContents* web_contents,
                      const SurveyBitsData& product_specific_bits_data,
                      const SurveyStringData& product_specific_string_data,
                      NavigationBehaviour navigation_behaviour,
                      base::OnceClosure success_callback,
                      base::OnceClosure failure_callback,
                      std::optional<std::string_view> supplied_trigger_id);

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
    virtual base::WeakPtr<DelayedSurveyTask> GetWeakPtr();

    bool operator<(const HatsServiceDesktop::DelayedSurveyTask& other) const {
      return trigger_ < other.trigger_ ? true
                                       : web_contents() < other.web_contents();
    }

   private:
    raw_ptr<HatsServiceDesktop> hats_service_;

    std::string trigger_;
    SurveyBitsData product_specific_bits_data_;
    SurveyStringData product_specific_string_data_;
    NavigationBehaviour navigation_behaviour_;
    base::OnceClosure success_callback_;
    base::OnceClosure failure_callback_;
    std::optional<std::string> supplied_trigger_id_;
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

  explicit HatsServiceDesktop(Profile* profile);

  HatsServiceDesktop(const HatsServiceDesktop&) = delete;
  HatsServiceDesktop& operator=(const HatsServiceDesktop&) = delete;

  ~HatsServiceDesktop() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void LaunchSurvey(
      const std::string& trigger,
      base::OnceClosure success_callback = base::DoNothing(),
      base::OnceClosure failure_callback = base::DoNothing(),
      const SurveyBitsData& product_specific_bits_data = {},
      const SurveyStringData& product_specific_string_data = {}) override;

  void LaunchSurveyForWebContents(
      const std::string& trigger,
      content::WebContents* web_contents,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data,
      base::OnceClosure success_callback = base::DoNothing(),
      base::OnceClosure failure_callback = base::DoNothing(),
      const std::optional<std::string>& supplied_trigger_id = std::nullopt,
      const SurveyOptions& survey_options = SurveyOptions()) override;

  bool LaunchDelayedSurvey(
      const std::string& trigger,
      int timeout_ms,
      const SurveyBitsData& product_specific_bits_data = {},
      const SurveyStringData& product_specific_string_data = {}) override;

  bool LaunchDelayedSurveyForWebContents(
      const std::string& trigger,
      content::WebContents* web_contents,
      int timeout_ms,
      const SurveyBitsData& product_specific_bits_data = {},
      const SurveyStringData& product_specific_string_data = {},
      NavigationBehaviour navigation_behaviour = NavigationBehaviour::ALLOW_ANY,
      base::OnceClosure success_callback = base::DoNothing(),
      base::OnceClosure failure_callback = base::DoNothing(),
      const std::optional<std::string>& supplied_trigger_id = std::nullopt,
      const SurveyOptions& survey_options = SurveyOptions()) override;

  void SetSurveyMetadataForTesting(const HatsService::SurveyMetadata& metadata);
  void GetSurveyMetadataForTesting(HatsService::SurveyMetadata* metadata) const;

  bool HasPendingTasks();

  bool CanShowSurvey(const std::string& trigger) const override;

  bool CanShowAnySurvey(bool user_prompted) const override;

  void RecordSurveyAsShown(std::string trigger_id) override;

  // Indicates to the service that the HaTS Next dialog has been closed.
  virtual void HatsNextDialogClosed();

  // Returns whether a HaTS Next dialog currently exists, regardless of whether
  // it is being shown or not.
  bool hats_next_dialog_exists_for_testing() {
    return hats_next_dialog_exists_;
  }

 protected:
 private:
  FRIEND_TEST_ALL_PREFIXES(HatsServiceProbabilityOne, SingleHatsNextDialog);

  // Remove |task| from the set of |pending_tasks_|.
  void RemoveTask(const DelayedSurveyTask& task);

  // Returns true is the survey trigger specified should be shown.
  bool ShouldShowSurvey(const std::string& trigger) const;

  void LaunchSurveyForBrowser(
      Browser* browser,
      const std::string& trigger,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data,
      const std::optional<std::string_view>& supplied_trigger_id =
          std::nullopt);

  // Check whether the survey is reachable and under capacity and show it.
  // |success_callback| is called when the survey is shown to the user.
  // |failure_callback| is called if the survey does not launch for any reason.
  // The matches of field names with the `SurveyConfig` are CHECK
  // enforced.
  void CheckSurveyStatusAndMaybeShow(
      Browser* browser,
      const std::string& trigger,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data,
      const std::optional<std::string_view>& supplied_trigger_id);

  std::set<DelayedSurveyTask> pending_tasks_;

  // Whether a HaTS Next dialog currently exists (regardless of whether it
  // is being shown to the user).
  bool hats_next_dialog_exists_ = false;

  base::WeakPtrFactory<HatsServiceDesktop> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_HATS_HATS_SERVICE_DESKTOP_H_
