// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/session/session_activation_observer.h"
#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/assistant_manager_service.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"

class GoogleServiceAuthError;
class PrefService;

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

namespace ash::assistant {

class AssistantInteractionLogger;
class ScopedAshSessionObserver;
class ServiceContext;

// |AssistantManagerService|'s state won't update if it's currently in the
// process of starting up. This is the delay before we will try to update
// |AssistantManagerService| again.
constexpr auto kUpdateAssistantManagerDelay = base::Seconds(1);

class COMPONENT_EXPORT(ASSISTANT_SERVICE) Service
    : public AssistantService,
      public chromeos::PowerManagerClient::Observer,
      public SessionActivationObserver,
      public AssistantStateObserver,
      public AssistantManagerService::StateObserver,
      public AuthenticationStateObserver {
 public:
  Service(std::unique_ptr<network::PendingSharedURLLoaderFactory>
              pending_url_loader_factory,
          signin::IdentityManager* identity_manager,
          PrefService* pref_service);

  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;

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

  // SessionActivationObserver overrides:
  void OnSessionActivated(bool activated) override;
  void OnLockStateChanged(bool locked) override;

  // AssistantStateObserver overrides:
  void OnAssistantConsentStatusChanged(int consent_status) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnAssistantHotwordAlwaysOn(bool hotword_always_on) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantHotwordEnabled(bool enabled) override;
  void OnLocaleChanged(const std::string& locale) override;
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnLockedFullScreenStateChanged(bool enabled) override;

  // AuthenticationStateObserver overrides:
  void OnAuthenticationError() override;

  // AssistantManagerService::StateObserver overrides:
  void OnStateChanged(AssistantManagerService::State new_state) override;

  void UpdateAssistantManagerState();
  void ScheduleUpdateAssistantManagerState(bool should_backoff);

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

  void OnLibassistantServiceRunning();
  void OnLibassistantServiceStopped();
  void OnLibassistantServiceDisconnected();

  void AddAshSessionObserver();

  void UpdateListeningState();

  std::optional<AssistantManagerService::UserInfo> GetUserInfo() const;

  ServiceContext* context() { return context_.get(); }

  // Returns the "actual" hotword status. In addition to the hotword pref, this
  // method also take power status into account if dsp support is not available
  // for the device.
  bool ShouldEnableHotword();

  void LoadLibassistant();
  void OnLibassistantLoaded(bool success);

  void ClearAfterStop();

  void DecreaseStartServiceBackoff();

  base::TimeDelta GetAutoRecoverTime();
  void SetAutoRecoverTimeForTesting(base::TimeDelta delay) {
    auto_recover_time_for_testing_ = delay;
  }

  bool CanStartService() const;

  void OnDataDeleted();

  // |ServiceContext| object passed to child classes so they can access some of
  // our functionality without depending on us.
  // Note: this is used by the other members here, so it must be defined first
  // so it is destroyed last.
  std::unique_ptr<ServiceContext> context_;

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<PrefService> pref_service_;
  std::unique_ptr<ScopedAshSessionObserver> scoped_ash_session_observer_;
  std::unique_ptr<AssistantManagerService> assistant_manager_service_;
  std::unique_ptr<base::OneShotTimer> token_refresh_timer_;
  int token_refresh_error_backoff_factor = 1;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};

  // Flag to guard the one-time mojom initialization.
  bool is_assistant_manager_service_finalized_ = false;
  // Whether the current user session is active.
  bool session_active_ = false;
  // Whether the lock screen is on.
  bool locked_ = false;
  // Whether the power source is connected.
  bool power_source_connected_ = false;
  // Whether the libassistant library is loaded.
  bool libassistant_loaded_ = false;
  // Whether is deleting data.
  bool is_deleting_data_ = false;

  // The value passed into |SetAssistantManagerServiceForTesting|.
  // Will be moved into |assistant_manager_service_| when the service is
  // supposed to be created.
  std::unique_ptr<AssistantManagerService>
      assistant_manager_service_for_testing_;

  std::optional<std::string> access_token_;

  // non-null until |assistant_manager_service_| is created.
  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;

  // If Libassistant service is disconnected, will use this backoff entry to
  // restart the service.
  net::BackoffEntry start_service_retry_backoff_;

  // A timer used to slowly recover from previous crashes by reducing the
  // `start_service_retry_backoff_` failure_count by one for every
  // `kAutoRecoverTime`.
  std::unique_ptr<base::OneShotTimer> auto_service_recover_timer_;
  base::TimeDelta auto_recover_time_for_testing_;

  base::CancelableOnceClosure update_assistant_manager_callback_;

  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  std::unique_ptr<AssistantInteractionLogger> interaction_logger_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Service> weak_ptr_factory_{this};
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_SERVICE_H_
