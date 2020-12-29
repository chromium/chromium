// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_SERVICE_H_
#define CHROMEOS_SERVICES_ASSISTANT_SERVICE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/session/session_activation_observer.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "mojo/public/cpp/bindings/remote.h"

class GoogleServiceAuthError;

namespace base {
class OneShotTimer;
}  // namespace base

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace power_manager {
class PowerSupplyProperties;
}  // namespace power_manager

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace chromeos {
namespace assistant {

class AssistantInteractionLogger;
class ScopedAshSessionObserver;
class ServiceContext;

// |AssistantManagerService|'s state won't update if it's currently in the
// process of starting up. This is the delay before we will try to update
// |AssistantManagerService| again.
constexpr auto kUpdateAssistantManagerDelay = base::TimeDelta::FromSeconds(1);

class COMPONENT_EXPORT(ASSISTANT_SERVICE) Service
    : public AssistantService,
      public chromeos::PowerManagerClient::Observer,
      public ash::SessionActivationObserver,
      public ash::AssistantStateObserver,
      public AssistantManagerService::CommunicationErrorObserver,
      public AssistantManagerService::StateObserver,
      public ash::AmbientUiModelObserver {
 public:
  Service(std::unique_ptr<network::PendingSharedURLLoaderFactory>
              pending_url_loader_factory,
          signin::IdentityManager* identity_manager);
  ~Service() override;

  // Allows tests to override the S3 server URI used by the service.
  // The caller must ensure the memory passed in remains valid.
  // This override can be removed by passing in a nullptr.
  // Note: This would look nicer if it was a class method and not static,
  // but unfortunately this must be called before |Service| tries to create the
  // |AssistantManagerService|, which happens really soon after the service
  // itself is created, so we do not have time in our tests to grab a handle
  // to |Service| and set this before it is too late.
  static void OverrideS3ServerUriForTesting(const char* uri);
  static void OverrideDeviceIdForTesting(const char* device_id);

  void SetAssistantManagerServiceForTesting(
      std::unique_ptr<AssistantManagerService> assistant_manager_service);

  // AssistantService overrides:
  void Init() override;
  void Shutdown() override;
  Assistant* GetAssistant() override;

 private:
  friend class AssistantServiceTest;

  class Context;

  // chromeos::PowerManagerClient::Observer overrides:
  void PowerChanged(const power_manager::PowerSupplyProperties& prop) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // ash::SessionActivationObserver overrides:
  void OnSessionActivated(bool activated) override;
  void OnLockStateChanged(bool locked) override;

  // ash::AssistantStateObserver overrides:
  void OnAssistantConsentStatusChanged(int consent_status) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnAssistantHotwordAlwaysOn(bool hotword_always_on) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantHotwordEnabled(bool enabled) override;
  void OnLocaleChanged(const std::string& locale) override;
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  // AssistantManagerService::CommunicationErrorObserver overrides:
  void OnCommunicationError(
      AssistantManagerService::CommunicationErrorType error_type) override;

  // AssistantManagerService::StateObserver overrides:
  void OnStateChanged(AssistantManagerService::State new_state) override;

  // ash::AmbientUiModelObserver overrides:
  void OnAmbientUiVisibilityChanged(
      ash::AmbientUiVisibility visibility) override;

  void UpdateAssistantManagerState();

  CoreAccountInfo RetrievePrimaryAccountInfo() const;
  void RequestAccessToken();
  void GetAccessTokenCallback(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info);
  void RetryRefreshToken();

  void CreateAssistantManagerService();
  std::unique_ptr<AssistantManagerService>
  CreateAndReturnAssistantManagerService();

  void FinalizeAssistantManagerService();

  void StopAssistantManagerService();

  void AddAshSessionObserver();

  void UpdateListeningState();

  base::Optional<AssistantManagerService::UserInfo> GetUserInfo() const;

  ServiceContext* context() { return context_.get(); }

  // Returns the "actual" hotword status. In addition to the hotword pref, this
  // method also take power status into account if dsp support is not available
  // for the device.
  bool ShouldEnableHotword();

  // |ServiceContext| object passed to child classes so they can access some of
  // our functionality without depending on us.
  // Note: this is used by the other members here, so it must be defined first
  // so it is destroyed last.
  std::unique_ptr<ServiceContext> context_;

  signin::IdentityManager* const identity_manager_;
  std::unique_ptr<ScopedAshSessionObserver> scoped_ash_session_observer_;
  ScopedObserver<ash::AmbientUiModel, ash::AmbientUiModelObserver>
      ambient_ui_model_observer_{this};
  std::unique_ptr<AssistantManagerService> assistant_manager_service_;
  std::unique_ptr<base::OneShotTimer> token_refresh_timer_;
  int token_refresh_error_backoff_factor = 1;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_observer_{this};

  // Flag to guard the one-time mojom initialization.
  bool is_assistant_manager_service_finalized_ = false;
  // Whether the current user session is active.
  bool session_active_ = false;
  // Whether the lock screen is on.
  bool locked_ = false;
  // Whether the power source is connected.
  bool power_source_connected_ = false;

  // The value passed into |SetAssistantManagerServiceForTesting|.
  // Will be moved into |assistant_manager_service_| when the service is
  // supposed to be created.
  std::unique_ptr<AssistantManagerService>
      assistant_manager_service_for_testing_ = nullptr;

  base::Optional<std::string> access_token_;

  // non-null until |assistant_manager_service_| is created.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;

  base::CancelableOnceClosure update_assistant_manager_callback_;

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  std::unique_ptr<AssistantInteractionLogger> interaction_logger_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Service> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_SERVICE_H_
