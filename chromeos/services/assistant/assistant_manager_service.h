// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/assistant_settings.h"
#include "chromeos/services/libassistant/public/mojom/authentication_state_observer.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom-shared.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"

namespace chromeos {
namespace assistant {

class AuthenticationStateObserver;

// Interface class that defines all assistant functionalities.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantManagerService
    : public Assistant {
 public:
  class StateObserver;

  struct UserInfo {
    UserInfo(const std::string& gaia_id, const std::string& access_token)
        : gaia_id(gaia_id), access_token(access_token) {}

    std::string gaia_id;
    std::string access_token;
  };

  using State = chromeos::libassistant::mojom::ServiceState;

  ~AssistantManagerService() override = default;

  // Start the Assistant in the background with the given |user|.
  // If the user is nullopt, the service will be started in signed-out mode.
  // If you want to know when the service is started, use
  // |AddAndFireStateObserver| to add an observer.
  virtual void Start(const base::Optional<UserInfo>& user,
                     bool enable_hotword) = 0;

  // Stop the Assistant.
  virtual void Stop() = 0;

  // Return the current state.
  virtual State GetState() const = 0;

  // Set user information for Assistant. Passing a nullopt will reconfigure
  // Libassistant to run in signed-out mode, and passing a valid non-empty value
  // will switch the mode back to normal.
  virtual void SetUser(const base::Optional<UserInfo>& user) = 0;

  // Turn on / off all listening, including hotword and voice query.
  virtual void EnableListening(bool enable) = 0;

  // Turn on / off hotword listening.
  virtual void EnableHotword(bool enable) = 0;

  // Enable/disable ARC play store.
  virtual void SetArcPlayStoreEnabled(bool enabled) = 0;

  // Enable/disable Assistant Context.
  virtual void SetAssistantContextEnabled(bool enable) = 0;

  // Return a pointer of AssistantSettings.
  virtual AssistantSettings* GetAssistantSettings() = 0;

  virtual void AddAuthenticationStateObserver(
      AuthenticationStateObserver* observer) = 0;

  // Add/Remove an observer that is invoked when there is a change in the
  // |AssistantManagerService::State| value.
  // When adding an observer it will immediately be triggered with the current
  // state value.
  virtual void AddAndFireStateObserver(StateObserver* observer) = 0;
  virtual void RemoveStateObserver(const StateObserver* observer) = 0;

  // Sync the device apps user consent status.
  virtual void SyncDeviceAppsStatus() = 0;

  // Update and sync the internal media player status to Libassistant.
  virtual void UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction action) = 0;
};

// Observes all state changes made to the |AssistantManagerService::State|.
class AssistantManagerService::StateObserver : public base::CheckedObserver {
 public:
  StateObserver() = default;
  ~StateObserver() override = default;

  virtual void OnStateChanged(AssistantManagerService::State new_state) = 0;
};

class AuthenticationStateObserver
    : public ::chromeos::libassistant::mojom::AuthenticationStateObserver {
 public:
  AuthenticationStateObserver();
  ~AuthenticationStateObserver() override;

  mojo::PendingRemote<
      ::chromeos::libassistant::mojom::AuthenticationStateObserver>
  BindNewPipeAndPassRemote();

 private:
  mojo::Receiver<::chromeos::libassistant::mojom::AuthenticationStateObserver>
      receiver_{this};
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_H_
