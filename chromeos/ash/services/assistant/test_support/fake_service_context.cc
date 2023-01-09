// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/services/assistant/public/cpp/device_actions.h"
#include "chromeos/ash/services/assistant/test_support/fake_service_context.h"

namespace ash::assistant {

/*static*/
constexpr const char* FakeServiceContext::kGaiaId;

FakeServiceContext::FakeServiceContext() = default;

FakeServiceContext::~FakeServiceContext() = default;

FakeServiceContext& FakeServiceContext::set_assistant_alarm_timer_controller(
    AssistantAlarmTimerController* value) {
  assistant_alarm_timer_controller_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_main_task_runner(
    scoped_refptr<base::SingleThreadTaskRunner> value) {
  main_task_runner_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_power_manager_client(
    chromeos::PowerManagerClient* value) {
  power_manager_client_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_primary_account_gaia_id(
    std::string value) {
  gaia_id_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_assistant_state(
    AssistantStateBase* value) {
  assistant_state_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_assistant_notification_controller(
    AssistantNotificationController* value) {
  assistant_notification_controller_ = value;
  return *this;
}

FakeServiceContext& FakeServiceContext::set_cras_audio_handler(
    CrasAudioHandler* value) {
  cras_audio_handler_ = value;
  return *this;
}

AssistantAlarmTimerController*
FakeServiceContext::assistant_alarm_timer_controller() {
  DCHECK(assistant_alarm_timer_controller_ != nullptr);
  return assistant_alarm_timer_controller_;
}

AssistantController* FakeServiceContext::assistant_controller() {
  NOTIMPLEMENTED();
  return nullptr;
}

AssistantNotificationController*
FakeServiceContext::assistant_notification_controller() {
  DCHECK(assistant_notification_controller_ != nullptr);
  return assistant_notification_controller_;
}

AssistantScreenContextController*
FakeServiceContext::assistant_screen_context_controller() {
  NOTIMPLEMENTED();
  return nullptr;
}

AssistantStateBase* FakeServiceContext::assistant_state() {
  DCHECK(assistant_state_ != nullptr);
  return assistant_state_;
}

CrasAudioHandler* FakeServiceContext::cras_audio_handler() {
  DCHECK(cras_audio_handler_ != nullptr);
  return cras_audio_handler_;
}

DeviceActions* FakeServiceContext::device_actions() {
  return DeviceActions::Get();
}

scoped_refptr<base::SequencedTaskRunner>
FakeServiceContext::main_task_runner() {
  DCHECK(main_task_runner_ != nullptr);
  return main_task_runner_;
}

chromeos::PowerManagerClient* FakeServiceContext::power_manager_client() {
  DCHECK(power_manager_client_ != nullptr);
  return power_manager_client_;
}

std::string FakeServiceContext::primary_account_gaia_id() {
  return gaia_id_;
}

}  // namespace ash::assistant
