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
#include "base/timer/timer.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace boca {
class UserIdentity;
class Bundle;
class CaptionsConfig;
}  // namespace boca

namespace ash::boca {

class BocaSessionManager
    : public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  // TODO(b/361852484): Make it 5 minutes after fcm in place.
  inline static constexpr base::TimeDelta kPollingInterval = base::Seconds(5);

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
                     AccountId account_id);
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
        const ::boca::CaptionsConfig& config);

    // Notifies when local caption config updated.
    virtual void OnLocalCaptionConfigUpdated(
        const ::boca::CaptionsConfig& config);

    // Notifies when session roster updated. Will emit when only elements order
    // changed in the vector too. Deferred to events consumer to decide on
    // the actual action.
    virtual void OnSessionRosterUpdated(
        const std::string& group_name,
        const std::vector<::boca::UserIdentity>& consumers);
  };
  // CrosNetworkConfigObserver
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state)
      override;

  void NotifyError(BocaError error);
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void StartSessionPolling();
  void LoadCurrentSession();
  void ParseSessionResponse(base::expected<std::unique_ptr<::boca::Session>,
                                           google_apis::ApiErrorCode> result);

  // Local events.
  virtual void NotifyLocalCaptionEvents(::boca::CaptionsConfig caption_config);

  base::ObserverList<Observer>& GetObserversForTesting();

 private:
  bool IsProfileActive();
  void NotifySessionUpdate();
  void NotifyOnTaskUpdate();
  void NotifyCaptionConfigUpdate();
  void NotifyRosterUpdate();

  base::ObserverList<Observer> observers_;
  // Timer used for periodic session polling.
  base::RepeatingTimer timer_;
  std::unique_ptr<::boca::Session> current_session_;
  std::unique_ptr<::boca::Session> previous_session_;
  bool is_network_conntected_ = false;
  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_{this};
  AccountId account_id_;
  raw_ptr<SessionClientImpl> session_client_impl_;
  base::WeakPtrFactory<BocaSessionManager> weak_factory_{this};
};
}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_
