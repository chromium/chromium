// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
class PlatformApi;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {
class LibassistantV1Api;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace assistant {
class AssistantManagerServiceDelegate;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

class ChromiumApiDelegate;

// Component managing the lifecycle of Libassistant,
// exposing methods to start/stop and configure Libassistant.
// Note: to access the Libassistant objects from //chromeos/services/assistant,
// use the |LibassistantV1Api| singleton, which will be populated by this class.
class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) ServiceController
    : public mojom::ServiceController {
 public:
  using InitializeCallback =
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>;

  ServiceController(assistant::AssistantManagerServiceDelegate* delegate,
                    assistant_client::PlatformApi* platform_api);
  ServiceController(ServiceController&) = delete;
  ServiceController& operator=(ServiceController&) = delete;
  ~ServiceController() override;

  void Bind(mojo::PendingReceiver<mojom::ServiceController> receiver);

  // Set a callback to initialize |AssistantManager| and
  // |AssistantManagerInternal|. This callback will be invoked before
  // AssistantManager::Start() is called. This is temporary until we've migrated
  // all initialization code to this class.
  void SetInitializeCallback(InitializeCallback callback);

  // mojom::ServiceController implementation:
  void Initialize(mojom::BootupConfigPtr libassistant_config,
                  mojo::PendingRemote<network::mojom::URLLoaderFactory>
                      url_loader_factory) override;
  void Start() override;
  void Stop() override;
  void ResetAllDataAndStop() override;
  void AddAndFireStateObserver(
      mojo::PendingRemote<mojom::StateObserver> observer) override;
  void SetSpokenFeedbackEnabled(bool value) override;
  void SetHotwordEnabled(bool value) override;
  void SetAuthenticationTokens(
      std::vector<mojom::AuthenticationTokenPtr> tokens) override;

  void AddAndFireAssistantManagerObserver(AssistantManagerObserver* observer);
  void RemoveAssistantManagerObserver(AssistantManagerObserver* observer);

  bool IsInitialized() const;
  // Note this is true even when the service is running (as it is still started
  // at that point).
  bool IsStarted() const;
  bool IsRunning() const;

  // Will return nullptr if the service is stopped.
  assistant_client::AssistantManager* assistant_manager();
  // Will return nullptr if the service is stopped.
  assistant_client::AssistantManagerInternal* assistant_manager_internal();

 private:
  class DeviceSettingsUpdater;
  class DeviceStateListener;

  using UpdateSettingsCallback = base::OnceCallback<void(const std::string&)>;
  void UpdateSettings(const std::string& settings,
                      UpdateSettingsCallback callback);

  void OnStartFinished();

  void SetStateAndInformObservers(mojom::ServiceState new_state);

  void SetLocale(const std::string& value);

  // The settings are being passed in to clearly document when the internal
  // options must be updated.
  void SetInternalOptions(const base::Optional<std::string>& locale,
                          base::Optional<bool> spoken_feedback_enabled);
  // The settings are being passed in to clearly document when the device
  // settings must be updated.
  void SetDeviceSettings(const base::Optional<std::string>& locale,
                         base::Optional<bool> hotword_enabled);

  void CreateAndRegisterDeviceStateListener();
  void CreateAndRegisterChromiumApiDelegate(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory);
  void CreateChromiumApiDelegate(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory);

  mojom::ServiceState state_ = mojom::ServiceState::kStopped;
  // The below options will be initialize during the Initialize() call.
  base::Optional<std::string> locale_;
  base::Optional<bool> spoken_feedback_enabled_;
  base::Optional<bool> hotword_enabled_;

  // Owned by |AssistantManagerServiceImpl| which indirectly owns us.
  assistant::AssistantManagerServiceDelegate* const delegate_;
  // Owned by |AssistantManagerServiceImpl| which indirectly owns us.
  assistant_client::PlatformApi* const platform_api_;

  // Callback called to initialize |AssistantManager| before it's started.
  InitializeCallback initialize_callback_;

  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;
  std::unique_ptr<ChromiumApiDelegate> chromium_api_delegate_;
  std::unique_ptr<assistant::LibassistantV1Api> libassistant_v1_api_;
  std::unique_ptr<DeviceStateListener> device_state_listener_;

  // Instantiated when |SetHotwordEnabled| is called.
  // Will wait until Libassistant is started, and then update the device
  // settings.
  std::unique_ptr<DeviceSettingsUpdater> device_settings_updater_;

  mojo::Receiver<mojom::ServiceController> receiver_;
  mojo::RemoteSet<mojom::StateObserver> state_observers_;
  base::ObserverList<AssistantManagerObserver> assistant_manager_observers_;
};

using ScopedAssistantManagerObserver = base::ScopedObservation<
    ServiceController,
    AssistantManagerObserver,
    &ServiceController::AddAndFireAssistantManagerObserver,
    &ServiceController::RemoveAssistantManagerObserver>;

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_
