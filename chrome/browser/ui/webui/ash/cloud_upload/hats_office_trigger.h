// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_HATS_OFFICE_TRIGGER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_HATS_OFFICE_TRIGGER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

namespace ash::cloud_upload {

// Timeout used to wait before displaying survey.
inline constexpr const base::TimeDelta kDelayTriggerTimeout = base::Minutes(1);

// Timeout to abandon the app state observation if the expected initial
// event hasn't been received within this delay.
inline constexpr const base::TimeDelta kFirstAppStateEventTimeout =
    base::Seconds(5);

// Debounce delay used when observing the change of state of apps.
inline constexpr const base::TimeDelta kDebounceDelay = base::Milliseconds(500);

// Recorded as survey metadata, indicates what app was used when the survey
// was launched.
enum class HatsOfficeLaunchingApp {
  kDrive,
  kMS365,
  kQuickOffice,
  kQuickOfficeClippyOff,
};

// Callback used in tests, called when the survey is attempted to be shown.
using ShowSurveyCallbackForTesting =
    base::OnceCallback<void(const std::string, HatsOfficeLaunchingApp)>;

// Used to show a Happiness Tracking Survey after user opens an Office file
// through the Files app.
class HatsOfficeTrigger {
 public:
  // Gets the global instance.
  static HatsOfficeTrigger& Get();

  HatsOfficeTrigger(const HatsOfficeTrigger&) = delete;
  HatsOfficeTrigger& operator=(const HatsOfficeTrigger&) = delete;

  void SetShowSurveyCallbackForTesting(ShowSurveyCallbackForTesting callback);

  // Trigger survey after a given delay, if the user is selected.
  void ShowSurveyAfterDelay(HatsOfficeLaunchingApp app);

  // Trigger survey once the given app becomes inactive or closed, if the
  // user is selected.
  void ShowSurveyAfterAppInactive(const std::string& app_id,
                                  HatsOfficeLaunchingApp app);

 private:
  friend class base::NoDestructor<HatsOfficeTrigger>;
  friend class HatsOfficeTriggerTestBase;

  struct DelayTrigger {
    explicit DelayTrigger(base::OnceClosure callback);

    base::OneShotTimer notification_timer_;
  };

  // Triggers its callback for an app to be closed or inactive.
  struct AppStateTrigger : public apps::InstanceRegistry::Observer {
    AppStateTrigger(Profile* profile,
                    const std::string& app_id,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback);
    ~AppStateTrigger() override;

    // apps::InstanceRegistry::Observer overrides.
    void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
    void OnInstanceRegistryWillBeDestroyed(
        apps::InstanceRegistry* cache) override;

    void HandleObservedAppStateUpdate(apps::InstanceState state);
    void StopTrackingAppState();

    const std::string app_id_;
    base::OnceClosure success_callback_;
    base::OnceClosure failure_callback_;
    base::UnguessableToken instance_id_;
    base::OneShotTimer first_app_state_event_timer_;
    base::OneShotTimer debounce_timer_;
    base::ScopedObservation<apps::InstanceRegistry,
                            apps::InstanceRegistry::Observer>
        observation_{this};
    base::WeakPtrFactory<AppStateTrigger> weak_ptr_factory_{this};
  };

  HatsOfficeTrigger();
  ~HatsOfficeTrigger();

  bool ShouldShowSurvey() const;

  const HatsNotificationController* GetHatsNotificationControllerForTesting()
      const;
  bool IsDelayTriggerActiveForTesting();
  bool IsAppStateTriggerActiveForTesting();

  Profile* GetProfile() const;

  void ShowSurveyIfSelected(HatsOfficeLaunchingApp app);
  void CleanupTriggers();

  std::unique_ptr<DelayTrigger> delay_trigger_;
  std::unique_ptr<AppStateTrigger> app_state_trigger_;
  ShowSurveyCallbackForTesting show_survey_callback_for_testing_;
  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;
  base::WeakPtrFactory<HatsOfficeTrigger> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_HATS_OFFICE_TRIGGER_H_
