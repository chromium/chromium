// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTEXT_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTEXT_H_

#include <string>

#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/assistant/service_context.h"

namespace chromeos {
namespace assistant {

// Fake implementation of the |ServiceContext| used during the unittests.
// Every method will assert when called,
// unless you've provided the object using one of the setter methods.
class FakeServiceContext : public ServiceContext {
 public:
  // Gaia ID returned by primary_account_gaia_id() (unless overridden).
  static constexpr const char* kGaiaId = "<fake-gaia-id>";

  FakeServiceContext();
  FakeServiceContext(const FakeServiceContext&) = delete;
  FakeServiceContext& operator=(const FakeServiceContext&) = delete;
  ~FakeServiceContext() override;

  FakeServiceContext& set_assistant_alarm_timer_controller(
      ash::AssistantAlarmTimerController*);
  FakeServiceContext& set_main_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner>);
  FakeServiceContext& set_power_manager_client(PowerManagerClient*);
  FakeServiceContext& set_primary_account_gaia_id(std::string);
  FakeServiceContext& set_assistant_state(ash::AssistantStateBase*);
  FakeServiceContext& set_assistant_notification_controller(
      ash::AssistantNotificationController*);
  FakeServiceContext& set_cras_audio_handler(ash::CrasAudioHandler*);

  // ServiceContext implementation:
  ash::AssistantAlarmTimerController* assistant_alarm_timer_controller()
      override;
  ash::AssistantController* assistant_controller() override;
  ash::AssistantNotificationController* assistant_notification_controller()
      override;
  ash::AssistantScreenContextController* assistant_screen_context_controller()
      override;
  ash::AssistantStateBase* assistant_state() override;
  ash::CrasAudioHandler* cras_audio_handler() override;
  DeviceActions* device_actions() override;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner() override;
  PowerManagerClient* power_manager_client() override;
  std::string primary_account_gaia_id() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  ash::AssistantStateBase* assistant_state_ = nullptr;
  PowerManagerClient* power_manager_client_ = nullptr;
  std::string gaia_id_ = kGaiaId;
  ash::AssistantAlarmTimerController* assistant_alarm_timer_controller_ =
      nullptr;
  ash::AssistantNotificationController* assistant_notification_controller_ =
      nullptr;
  ash::CrasAudioHandler* cras_audio_handler_ = nullptr;
};
}  // namespace assistant
}  // namespace chromeos
#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTEXT_H_
