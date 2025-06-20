// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"

namespace boca {
class UserIdentity;
class Bundle;
class CaptionsConfig;
}  // namespace boca

namespace google_apis {
enum ApiErrorCode;
}

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace ash::boca {

class BocaSessionManager
    : public chromeos::network_config::CrosNetworkConfigObserver,
      public signin::IdentityManager::Observer,
      public user_manager::UserManager::UserSessionStateObserver,
      public session_manager::SessionManagerObserver,
      public remoting::ClientStatusObserver {
 public:
  using SessionCaptionInitializer =
      base::RepeatingCallback<void(base::OnceCallback<void(bool)>)>;
  using SodaStatus = babelorca::SodaInstaller::InstallationStatus;

  inline static constexpr char kDummyDeviceId[] = "kDummyDeviceId";
  inline static constexpr int kDefaultPollingIntervalInSeconds = 60;
  inline static constexpr int kLocalSessionTrackerBufferInSeconds = 60;
  inline static constexpr int kDefaultStudentHeartbeatIntervalInSeconds = 30;
  inline static constexpr int kSkipPollingBufferInSeconds = 2;

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
                     const PrefService* pref_service,
                     AccountId account_id,
                     bool is_producer,
                     std::unique_ptr<SpotlightRemotingClientManager>
                         remoting_client_manager = nullptr);
  BocaSessionManager(const BocaSessionManager&) = delete;
  BocaSessionManager& operator=(const BocaSessionManager&) = delete;
  ~BocaSessionManager() override;

  // Interface for observing events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSessionMetadataUpdated(const std::string& session_id);
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

    // Notifies when local caption is disabled from a source other than the boca
    // app.
    virtual void OnLocalCaptionClosed();

    // Notifies when the status of SODA changes.
    virtual void OnSodaStatusUpdate(SodaStatus status);

    // Notifies when session caption is disabled from a source other than the
    // boca app.
    virtual void OnSessionCaptionClosed(bool is_error);

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
  void OnStudentHeartbeat(
      base::expected<bool, google_apis::ApiErrorCode> result);

  virtual void UpdateCurrentSession(std::unique_ptr<::boca::Session> session,
                                    bool dispatch_event);
  virtual ::boca::Session* GetCurrentSession();
  virtual const ::boca::Session* GetPreviousSession();

  virtual void UpdateTabActivity(std::u16string title);

  virtual void OnAppWindowOpened();

  // session_manager::SessionManagerObserver::Observer
  void OnSessionStateChanged() override;

  // Local events.
  virtual void NotifyLocalCaptionEvents(::boca::CaptionsConfig caption_config);

  virtual void NotifyLocalCaptionClosed();

  virtual void NotifySessionCaptionProducerEvents(
      const ::boca::CaptionsConfig& caption_config);

  // Triggered by SWA delegate to notify app reload events.
  virtual void NotifyAppReload();

  virtual bool disabled_on_non_managed_network();

  void SetSessionCaptionInitializer(
      SessionCaptionInitializer session_caption_initializer);
  void RemoveSessionCaptionInitializer();
  void InitSessionCaption(base::OnceCallback<void(bool)> success_cb);
  void SetSodaInstaller(babelorca::SodaInstaller* soda_installer) {
    soda_installer_ = soda_installer;
  }
  SodaStatus GetSodaStatus();

  void StartCrdClient(
      std::string crd_connection_code,
      base::OnceClosure done_callback,
      SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
      SpotlightCrdStateUpdatedCallback crd_state_callback);

  // Calls the `SpotlightRemotingClientManager` to try and stop an existing
  // session and then free up any remaining resources.
  void EndSpotlightSession();

  virtual std::string GetDeviceRobotEmail();

  base::ObserverList<Observer>& observers() { return observers_; }

  AccountId& account_id() { return account_id_; }

  SessionClientImpl* session_client_impl() { return session_client_impl_; }

  base::OneShotTimer& session_duration_timer_for_testing() {
    return session_duration_timer_;
  }

  base::OnceClosure& end_session_callback_for_testing() {
    return end_session_callback_for_testing_;
  }

  void set_end_session_callback_for_testing(base::OnceClosure cb) {
    end_session_callback_for_testing_ = std::move(cb);
  }

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
  void HandleTakeOver(bool dispatch_event,
                      std::unique_ptr<::boca::Session> session);
  void DispatchEvent();
  void NotifySessionUpdate();
  void NotifySessionMetadataUpdate();
  void NotifyOnTaskUpdate();
  void NotifySessionCaptionConfigUpdate();
  void NotifyRosterUpdate();
  void NotifyConsumerActivityUpdate();
  void HandleSessionUpdate(std::unique_ptr<::boca::Session> previous_session,
                           std::unique_ptr<::boca::Session> current_session,
                           bool dispatch_event);
  void UpdateLocalSessionDurationTracker();
  void StartSendingStudentHeartbeatRequests();
  void StopSendingStudentHeartbeatRequests();
  void SendStudentHeartbeatRequest();
  void HandleCaptionNotification();
  void UpdateNetworkRestriction(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state);
  void NotifySodaStatusListeners(SodaStatus status);

  void CloseAllCaptions();

  const bool is_producer_;
  base::OnceClosure end_session_callback_for_testing_;
  base::TimeDelta in_session_polling_interval_;
  base::TimeDelta indefinite_polling_interval_;
  base::ObserverList<Observer> observers_;
  // Timer used for periodic session polling within session.
  base::RepeatingTimer in_session_timer_;
  // Timer used for indefinite session polling.
  base::RepeatingTimer indefinite_timer_;
  // Timer used for tracking session duration on client. This is to make sure we
  // still end the session in time if device lose network access.
  base::OneShotTimer session_duration_timer_;
  base::TimeTicks last_session_load_;
  // Tracking session start time from remote. Use remote session start time
  // instead of local timesticks since device don't always join when session
  // start. The calculation used by this time will be subject to device drift,
  // but is in sync with the UI remaining time.
  base::Time last_session_start_time_;
  base::TimeDelta last_session_duration_;

  base::TimeDelta student_heartbeat_interval_;

  // Timer used for student heartbeat.
  base::RepeatingTimer student_heartbeat_timer_;
  // Timer used for student heartbeat exponential backoff.
  base::OneShotTimer student_heartbeat_backoff_timer_;

  std::unique_ptr<::boca::Session> current_session_;
  std::unique_ptr<::boca::Session> previous_session_;
  bool is_network_connected_ = false;
  bool disabled_on_non_managed_network_ = false;
  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};
  AccountId account_id_;
  std::u16string active_tab_title_;
  BocaNotificationHandler notification_handler_;
  std::unique_ptr<SpotlightRemotingClientManager> remoting_client_manager_;
  raw_ptr<const PrefService> pref_service_;
  raw_ptr<SessionClientImpl> session_client_impl_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<babelorca::SodaInstaller> soda_installer_;
  bool is_local_caption_enabled_ = false;
  SessionCaptionInitializer session_caption_initializer_;
  net::BackoffEntry student_heartbeat_retry_backoff_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
  base::WeakPtrFactory<BocaSessionManager> weak_factory_{this};
};
}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_
