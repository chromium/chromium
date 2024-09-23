// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_HATS_HATS_SERVICE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_HATS_HATS_SERVICE_ANDROID_H_

#include <memory>
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
#include "components/messages/android/message_enums.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace messages {
class MessageWrapper;
}

// The name of the histogram which records if a survey was shown, or if not, the
// reason why not.
extern const char kHatsShouldShowSurveyReasonAndroidHistogram[];

// This class provides the client side logic for determining if a
// survey should be shown for any trigger based on input from a finch
// configuration. It is created on a per profile basis.
class HatsServiceAndroid : public HatsService {
 public:
  class DelayedSurveyTask : public content::WebContentsObserver {
   public:
    DelayedSurveyTask(HatsServiceAndroid* hats_service,
                      const std::string& trigger,
                      content::WebContents* web_contents,
                      const SurveyBitsData& product_specific_bits_data,
                      const SurveyStringData& product_specific_string_data,
                      base::OnceClosure success_callback,
                      base::OnceClosure failure_callback,
                      const std::optional<std::string>& supplied_trigger_id,
                      const SurveyOptions& survey_options);

    // Not copyable or movable
    DelayedSurveyTask(const DelayedSurveyTask&) = delete;
    DelayedSurveyTask& operator=(const DelayedSurveyTask&) = delete;

    ~DelayedSurveyTask() override;

    // Asks |hats_service_| to launch the survey with id |trigger_| for tab
    // |web_contents_|.
    void Launch();

    void DismissCallback(messages::DismissReason reason);

    // content::WebContentsObserver
    void WebContentsDestroyed() override;

    // Returns a weak pointer to this object.
    base::WeakPtr<DelayedSurveyTask> GetWeakPtr();

    bool operator<(const HatsServiceAndroid::DelayedSurveyTask& other) const {
      return trigger_ < other.trigger_ ? true
                                       : web_contents() < other.web_contents();
    }

    messages::MessageWrapper* GetMessageForTesting() { return message_.get(); }

   private:
    raw_ptr<HatsServiceAndroid> hats_service_;

    std::unique_ptr<messages::MessageWrapper> message_;
    std::string trigger_;
    SurveyBitsData product_specific_bits_data_;
    SurveyStringData product_specific_string_data_;
    base::OnceClosure success_callback_;
    base::OnceClosure failure_callback_;
    std::optional<std::string> supplied_trigger_id_;
    SurveyOptions survey_options_;
    base::WeakPtrFactory<DelayedSurveyTask> weak_ptr_factory_{this};
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ShouldShowSurveyReasonsAndroid {
    kYes = 0,
    kAndroidUnknown = 1,   // Catch all for Android invitation dismissals.
                           // Should be investigated if this regularly occurs.
    kAndroidAccepted = 2,  // Invitation accepted
    kAndroidSecondaryAction =
        3,  // Not in use by the default survey implementation. May be used by
            // customized trigger implementations.
    kAndroidExpired = 4,  // Survey invitation expired and was automatically
                          // dismissed. Default timeout is 10s, see
                          // `ChromeMessageAutodismissDurationProvider.java`
    kAndroidDismissedByGesture = 5,  // Dismissed by swiping the dialog away
    kAndroidTabSwitched = 6,
    kAndroidTabDestroyed = 7,
    kAndroidActivityDestroyed = 8,
    kAndroidScopeDestroyed = 9,
    kAndroidDismissedByFeature =
        10,  // Another survey was already launched, leading to the current one
             // being aborted.
    kMaxValue = kAndroidDismissedByFeature,
  };

  explicit HatsServiceAndroid(Profile* profile);

  HatsServiceAndroid(const HatsServiceAndroid&) = delete;
  HatsServiceAndroid& operator=(const HatsServiceAndroid&) = delete;

  ~HatsServiceAndroid() override;

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

  // Currently not implemented
  bool CanShowAnySurvey(bool user_prompted) const override;

  // Currently not implemented
  bool CanShowSurvey(const std::string& trigger) const override;

  void RecordSurveyAsShown(std::string trigger_id) override;

  DelayedSurveyTask& GetFirstTaskForTesting() {
    return const_cast<DelayedSurveyTask&>(*pending_tasks_.begin());
  }

 protected:
  // Remove |task| from the set of |pending_tasks_|.
  void RemoveTask(const DelayedSurveyTask& task);

 private:
  friend class DelayedSurveyTask;
  FRIEND_TEST_ALL_PREFIXES(HatsServiceProbabilityOne, SingleHatsNextDialog);

  std::set<DelayedSurveyTask> pending_tasks_;
  base::WeakPtrFactory<HatsServiceAndroid> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_HATS_HATS_SERVICE_ANDROID_H_
