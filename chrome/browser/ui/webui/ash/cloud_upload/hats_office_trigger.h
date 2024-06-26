// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_HATS_OFFICE_TRIGGER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_HATS_OFFICE_TRIGGER_H_

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"

namespace ash::cloud_upload {

// Timeout used to wait before displaying survey.
inline constexpr const base::TimeDelta kHatsSurveyTimeout = base::Minutes(1);

// Recorded as survey metadata, indicates what app was used when the survey was
// launched.
enum class HatsOfficeLaunchingApp {
  kDrive,
  kMS365,
  kQuickOffice,
  kQuickOfficeClippyOff,
};

// Callback used in tests, called when the survey is attempted to be shown.
using ShowSurveyCallbackForTesting =
    base::OnceCallback<void(HatsOfficeLaunchingApp)>;

// Used to show a Happiness Tracking Survey after user opens an Office file
// through the Files app.
class HatsOfficeTrigger {
 public:
  // Gets the global instance.
  static HatsOfficeTrigger& Get();

  HatsOfficeTrigger(const HatsOfficeTrigger&) = delete;
  HatsOfficeTrigger& operator=(const HatsOfficeTrigger&) = delete;

  // Start the delay to show the survey to the user if they are selected.
  void ShowSurveyAfterDelay(HatsOfficeLaunchingApp app);

  void SetShowSurveyAfterDelayCallbackForTesting(
      ShowSurveyCallbackForTesting callback);

 private:
  friend class base::NoDestructor<HatsOfficeTrigger>;
  friend class HatsOfficeTriggerTest;

  HatsOfficeTrigger();
  ~HatsOfficeTrigger();

  const HatsNotificationController* GetHatsNotificationControllerForTesting()
      const;
  base::OneShotTimer& GetTimerForTesting();
  Profile* GetProfile() const;

  void ShowSurveyIfSelected(HatsOfficeLaunchingApp app);

  ShowSurveyCallbackForTesting show_survey_callback_for_testing_;
  base::OneShotTimer hats_notification_timer_;
  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;
  base::WeakPtrFactory<HatsOfficeTrigger> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_HATS_OFFICE_TRIGGER_H_
