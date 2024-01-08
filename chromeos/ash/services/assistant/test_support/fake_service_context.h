// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTEXT_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTEXT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/assistant/service_context.h"

namespace ash::assistant {

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
      AssistantAlarmTimerController*);
  FakeServiceContext& set_main_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner>);
  FakeServiceContext& set_power_manager_client(chromeos::PowerManagerClient*);
  FakeServiceContext& set_primary_account_gaia_id(std::string);
  FakeServiceContext& set_assistant_state(AssistantStateBase*);
  FakeServiceContext& set_assistant_notification_controller(
      AssistantNotificationController*);
  FakeServiceContext& set_cras_audio_handler(CrasAudioHandler*);

  // ServiceContext implementation:
  AssistantAlarmTimerController* assistant_alarm_timer_controller() override;
  AssistantController* assistant_controller() override;
  AssistantNotificationController* assistant_notification_controller() override;
  AssistantScreenContextController* assistant_screen_context_controller()
      override;
  AssistantStateBase* assistant_state() override;
  CrasAudioHandler* cras_audio_handler() override;
  DeviceActions* device_actions() override;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner() override;
  chromeos::PowerManagerClient* power_manager_client() override;
  std::string primary_account_gaia_id() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  raw_ptr<AssistantStateBase> assistant_state_ = nullptr;
  raw_ptr<chromeos::PowerManagerClient, DanglingUntriaged>
      power_manager_client_ = nullptr;
  std::string gaia_id_ = kGaiaId;
  raw_ptr<AssistantAlarmTimerController> assistant_alarm_timer_controller_ =
      nullptr;
  raw_ptr<AssistantNotificationController> assistant_notification_controller_ =
      nullptr;
  raw_ptr<CrasAudioHandler> cras_audio_handler_ = nullptr;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_SERVICE_CONTEXT_H_
