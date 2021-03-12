// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_
#define CHROMEOS_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_

#include <string>

#include "ash/public/cpp/assistant/controller/assistant_screen_context_controller.h"
// TODO(https://crbug.com/1164001): use forward declaration when migrated to
// ash/.
#include "ash/components/audio/cras_audio_handler.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
class AssistantAlarmTimerController;
class AssistantController;
class AssistantNotificationController;
class AssistantStateBase;
}  // namespace ash

namespace chromeos {
class PowerManagerClient;
}  // namespace chromeos

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromeos {
namespace assistant {

class DeviceActions;

// Context object passed around so classes can access some of the |Service|
// functionality without directly depending on the |Service| class.
class ServiceContext {
 public:
  virtual ~ServiceContext() = default;

  virtual ash::AssistantAlarmTimerController*
  assistant_alarm_timer_controller() = 0;

  virtual ash::AssistantController* assistant_controller() = 0;

  virtual ash::AssistantNotificationController*
  assistant_notification_controller() = 0;

  virtual ash::AssistantScreenContextController*
  assistant_screen_context_controller() = 0;

  virtual ash::AssistantStateBase* assistant_state() = 0;

  virtual ash::CrasAudioHandler* cras_audio_handler() = 0;

  virtual DeviceActions* device_actions() = 0;

  virtual scoped_refptr<base::SequencedTaskRunner> main_task_runner() = 0;

  virtual PowerManagerClient* power_manager_client() = 0;

  // Returns the Gaia ID of the primary account (which is used by the
  // Assistant).
  virtual std::string primary_account_gaia_id() = 0;
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_
