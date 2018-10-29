// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "base/callback_forward.h"
#include "chromeos/services/assistant/assistant_settings_manager.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"

namespace chromeos {
namespace assistant {

// Interface class that defines all assistant functionalities.
class AssistantManagerService : public mojom::Assistant {
 public:
  enum State {
    // Initial state, the service is created but not started yet.
    STOPPED = 0,
    // The service is started, it takes a little time to be fully running.
    STARTED = 1,
    // The service is fully running and ready to take requests.
    RUNNING = 2
  };

  ~AssistantManagerService() override = default;

  // Start the assistant in the background with |token|. When the service is
  // fully started |callback| will be called on the thread where ctor was run.
  virtual void Start(const std::string& access_token,
                     base::OnceClosure callback) = 0;

  // Stop the assistant.
  virtual void Stop() = 0;

  // Returns the current state.
  virtual State GetState() const = 0;

  // Set access token for assistant.
  virtual void SetAccessToken(const std::string& access_token) = 0;

  // Turn on / off hotword listening.
  virtual void EnableListening(bool enable) = 0;

  // Returns a pointer of AssistantSettingsManager.
  virtual AssistantSettingsManager* GetAssistantSettingsManager() = 0;

  using GetSettingsUiResponseCallback =
      base::OnceCallback<void(const std::string&)>;
  // Send request for getting settings ui.
  virtual void SendGetSettingsUiRequest(
      const std::string& selector,
      GetSettingsUiResponseCallback callback) = 0;

  using UpdateSettingsUiResponseCallback =
      base::OnceCallback<void(const std::string&)>;
  // Send request for updating settings ui.
  virtual void SendUpdateSettingsUiRequest(
      const std::string& update,
      UpdateSettingsUiResponseCallback callback) = 0;

  // Starts speaker id enrollment.
  virtual void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      mojom::SpeakerIdEnrollmentClientPtr client) = 0;

  // Stops speaker id enrollment (if one is active).
  virtual void StopSpeakerIdEnrollment(
      AssistantSettingsManager::StopSpeakerIdEnrollmentCallback callback) = 0;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
