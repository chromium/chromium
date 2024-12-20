// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace boca {
class UserIdentity;
class Bundle;
class CaptionsConfig;
}  // namespace boca

namespace google_apis {
enum ApiErrorCode;
}

namespace ash::boca {

class BocaSessionManager
    : public chromeos::network_config::CrosNetworkConfigObserver,
      public signin::IdentityManager::Observer,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  inline static constexpr char kDummyDeviceId[] = "kDummyDeviceId";
  inline static constexpr char kHomePageTitle[] = "School Tools Home page";
  inline static constexpr int kDefaultPollingIntervalInSeconds = 60;
  inline static constexpr char kPollingResultHistName[] =
      "Ash.Boca.PollingResult";

  enum class BocaAction {
    kDefault = 0,
    kOntask = 1,
    kLiveCaption = 2,
    kTranslation = 3,
    kTranscription = 4,
  };

  enum ErrorLevel {
    kInfo = 0,
    kWarn = 1,
    kFatal = 2,
  };

  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // `BocaPollingResult` in src/tools/metrics/histograms/metadata/ash/enums.xml.
  enum class BocaPollingResult {
    kNoUpdate = 0,
    kSessionStart = 1,
    kSessionEnd = 2,
    kInSessionUpdate = 3,
    kMaxValue = kInSessionUpdate,
  };

  struct BocaError {
    BocaError(BocaAction action_param,
              ErrorLevel error_level_param,
              std::string error_message_param)
        : action(action_param),
          error_level(error_level_param),
          error_message(error_message_param) {}
    const BocaAction action;
    const ErrorLevel error_level;
    const std::string error_message;
  };

  BocaSessionManager(SessionClientImpl* session_client_impl,
                     AccountId account_id,
                     bool is_producer);
  BocaSessionManager(const BocaSessionManager&) = delete;
  BocaSessionManager& operator=(const BocaSessionManager&) = delete;
  ~BocaSessionManager() override;

  // Interface for observing events.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies when session started. Pure virtual function, must be handled by
    // observer. Session metadata will be provided when fired.
    virtual void OnSessionStarted(const std::string& session_id,
                                  const ::boca::UserIdentity& producer) = 0;

    // Notifies when session ended. Pure virtual function, must be handled by
    // observer.
    virtual void OnSessionEnded(const std::string& session_id) = 0;

    // Notifies when bundle updated. In the event of session started with a
    // bundle configured, both events will be fired. Will emit when only
    // elements order changed in the vector too. Deferred to events consumer to
    // decide on the actual action.
    virtual void OnBundleUpdated(const ::boca::Bundle& bundle);

    // Notifies when session config updated for specific group.
    virtual void OnSessionCaptionConfigUpdated(
        const std::string& group_name,
        const ::boca::CaptionsConfig& config,
        const std::string& tachyon_group_id);

    // Notifies when local caption config updated.
    virtual void OnLocalCaptionConfigUpdated(
        const ::boca::CaptionsConfig& config);

    // Notifies when session roster updated. Will emit when only elements order
    // changed in the vector too. Deferred to events consumer to decide on
    // the actual action.
    virtual void OnSessionRosterUpdated(const ::boca::Roster& roster);

    // Notifies when boca app reloaded.
    virtual void OnAppReloaded();

    // Notifies when consumer activity updated. Will emit when only elements
    // order changed in the vector too.
    virtual void OnConsumerActivityUpdated(
        const std::map<std::string, ::boca::StudentStatus>& activities);
  };
  // CrosNetworkConfigObserver
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state)
      override;

  // signin::IdentityManager::Observer
  void OnRefreshTokenUpdatedForAccount(const CoreAccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  void NotifyError(BocaError error);
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void StartSessionPolling(bool in_session);
  void MaybeLoadCurrentSession();
  virtual void LoadCurrentSession(bool from_polling);
  void ParseSessionResponse(bool from_polling,
                            base::expected<std::unique_ptr<::boca::Session>,
                                           google_apis::ApiErrorCode> result);

  virtual void UpdateCurrentSession(std::unique_ptr<::boca::Session> session,
                                    bool dispatch_event);
  virtual ::boca::Session* GetCurrentSession();
  virtual const ::boca::Session* GetPreviousSession();

  virtual void UpdateTabActivity(std::u16string title);

  virtual void ToggleAppStatus(bool is_app_opened);

  // Local events.
  virtual void NotifyLocalCaptionEvents(::boca::CaptionsConfig caption_config);

  // Triggered by SWA delegate to notify app reload events.
  virtual void NotifyAppReload();

  base::ObserverList<Observer>& observers() { return observers_; }

  AccountId& account_id() { return account_id_; }

  SessionClientImpl* session_client_impl() { return session_client_impl_; }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void LoadInitialNetworkState();
  void OnNetworkStateFetched(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);
  bool IsProfileActive();
  bool IsSessionActive(const ::boca::Session* session);
  bool IsSessionTakeOver(const ::boca::Session* previous_session,
                         const ::boca::Session* current_session);
  void RecordPollingResult(const ::boca::Session* previous_session,
                           const ::boca::Session* current_session);
  void HandleTakeOver(bool dispatch_event,
                      std::unique_ptr<::boca::Session> session);
  void DispatchEvent();
  void NotifySessionUpdate();
  void NotifyOnTaskUpdate();
  void NotifySessionCaptionConfigUpdate();
  void NotifyRosterUpdate();
  void NotifyConsumerActivityUpdate();

  const bool is_producer_;
  bool is_app_opened_ = false;
  base::TimeDelta in_session_polling_interval_;
  base::TimeDelta indefinite_polling_interval_;
  base::ObserverList<Observer> observers_;
  // Timer used for periodic session polling within session.
  base::RepeatingTimer in_session_timer_;
  // Timer used for indefinite session polling.
  base::RepeatingTimer indefinite_timer_;
  base::TimeTicks last_session_load_;
  std::unique_ptr<::boca::Session> current_session_;
  std::unique_ptr<::boca::Session> previous_session_;
  bool is_network_connected_ = false;
  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};
  AccountId account_id_;
  std::u16string active_tab_title_;
  BocaNotificationHandler notification_handler_;
  raw_ptr<SessionClientImpl> session_client_impl_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  bool is_local_caption_enabled_ = false;
  base::WeakPtrFactory<BocaSessionManager> weak_factory_{this};
};
}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_
