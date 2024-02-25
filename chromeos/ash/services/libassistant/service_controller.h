// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/service_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/settings_controller.mojom-forward.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::libassistant {

class ChromiumApiDelegate;
class LibassistantFactory;

// Component managing the lifecycle of Libassistant,
// exposing methods to start/stop and configure Libassistant.
class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) ServiceController
    : public mojom::ServiceController,
      public ServicesStatusObserver {
 public:
  explicit ServiceController(LibassistantFactory* factory);
  ServiceController(ServiceController&) = delete;
  ServiceController& operator=(ServiceController&) = delete;
  ~ServiceController() override;

  void Bind(mojo::PendingReceiver<mojom::ServiceController> receiver,
            mojom::SettingsController* settings_controller);

  // mojom::ServiceController implementation:
  void Initialize(mojom::BootupConfigPtr libassistant_config,
                  mojo::PendingRemote<network::mojom::URLLoaderFactory>
                      url_loader_factory) override;
  void Start() override;
  void Stop() override;
  void ResetAllDataAndStop() override;
  void AddAndFireStateObserver(
      mojo::PendingRemote<mojom::StateObserver> observer) override;

  // ServicesStatusObserver implementation:
  void OnServicesStatusChanged(ServicesStatus status) override;

  void AddAndFireAssistantClientObserver(AssistantClientObserver* observer);
  void RemoveAssistantClientObserver(AssistantClientObserver* observer);
  void RemoveAllAssistantClientObservers();

  bool IsInitialized() const;
  // Note this is true even when the service is running (as it is still started
  // at that point).
  bool IsStarted() const;
  bool IsRunning() const;

  // Will return nullptr if the service is stopped.
  AssistantClient* assistant_client();

 private:
  // Will be invoked when all Libassistant services are ready to query.
  void OnAllServicesReady();
  // Will be invoked when Libassistant services are started.
  void OnServicesBootingUp();

  void SetStateAndInformObservers(mojom::ServiceState new_state);

  void CreateAndRegisterChromiumApiDelegate(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory);
  void CreateChromiumApiDelegate(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory);

  mojom::ServiceState state_ = mojom::ServiceState::kStopped;

  // Called during |Initialize| to apply boot configuration.
  raw_ptr<mojom::SettingsController> settings_controller_ = nullptr;

  const raw_ref<LibassistantFactory> libassistant_factory_;

  std::unique_ptr<AssistantClient> assistant_client_;
  std::unique_ptr<ChromiumApiDelegate> chromium_api_delegate_;

  mojo::Receiver<mojom::ServiceController> receiver_{this};
  mojo::RemoteSet<mojom::StateObserver> state_observers_;
  base::ObserverList<AssistantClientObserver> assistant_client_observers_;

  base::WeakPtrFactory<ServiceController> weak_factory_{this};
};

}  // namespace ash::libassistant

namespace base {

template <>
struct ScopedObservationTraits<ash::libassistant::ServiceController,
                               ash::libassistant::AssistantClientObserver> {
  static void AddObserver(
      ash::libassistant::ServiceController* source,
      ash::libassistant::AssistantClientObserver* observer) {
    source->AddAndFireAssistantClientObserver(observer);
  }
  static void RemoveObserver(
      ash::libassistant::ServiceController* source,
      ash::libassistant::AssistantClientObserver* observer) {
    source->RemoveAssistantClientObserver(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_SERVICE_CONTROLLER_H_
