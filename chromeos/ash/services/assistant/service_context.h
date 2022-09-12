// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_

#include <string>

#include "ash/public/cpp/assistant/controller/assistant_screen_context_controller.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromeos {
class PowerManagerClient;
}

namespace ash {

class AssistantAlarmTimerController;
class AssistantController;
class AssistantNotificationController;
class AssistantStateBase;
class CrasAudioHandler;
class DeviceActions;

namespace assistant {

// Context object passed around so classes can access some of the |Service|
// functionality without directly depending on the |Service| class.
class ServiceContext {
 public:
  virtual ~ServiceContext() = default;

  virtual AssistantAlarmTimerController* assistant_alarm_timer_controller() = 0;

  virtual AssistantController* assistant_controller() = 0;

  virtual AssistantNotificationController*
  assistant_notification_controller() = 0;

  virtual AssistantScreenContextController*
  assistant_screen_context_controller() = 0;

  virtual AssistantStateBase* assistant_state() = 0;

  virtual CrasAudioHandler* cras_audio_handler() = 0;

  virtual DeviceActions* device_actions() = 0;

  virtual scoped_refptr<base::SequencedTaskRunner> main_task_runner() = 0;

  virtual chromeos::PowerManagerClient* power_manager_client() = 0;

  // Returns the Gaia ID of the primary account (which is used by the
  // Assistant).
  virtual std::string primary_account_gaia_id() = 0;
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_
