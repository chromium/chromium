// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_
#define CHROMEOS_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_

#include "base/memory/scoped_refptr.h"

namespace ash {
class AssistantStateBase;

namespace mojom {
class AssistantAlarmTimerController;
class AssistantNotificationController;
class AssistantScreenContextController;
}  // namespace mojom
}  // namespace ash

namespace chromeos {
class CrasAudioHandler;
class PowerManagerClient;

namespace assistant {
namespace mojom {
class AssistantController;
class DeviceActions;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromeos {
namespace assistant {

// Context object passed around so classes can access some of the |Service|
// functionality without directly depending on the |Service| class.
class ServiceContext {
 public:
  virtual ~ServiceContext() = default;

  virtual ash::mojom::AssistantAlarmTimerController*
  assistant_alarm_timer_controller() = 0;

  virtual mojom::AssistantController* assistant_controller() = 0;

  virtual ash::mojom::AssistantNotificationController*
  assistant_notification_controller() = 0;

  virtual ash::mojom::AssistantScreenContextController*
  assistant_screen_context_controller() = 0;

  virtual ash::AssistantStateBase* assistant_state() = 0;

  virtual CrasAudioHandler* cras_audio_handler() = 0;

  virtual mojom::DeviceActions* device_actions() = 0;

  virtual scoped_refptr<base::SequencedTaskRunner> main_task_runner() = 0;

  virtual PowerManagerClient* power_manager_client() = 0;
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_SERVICE_CONTEXT_H_
