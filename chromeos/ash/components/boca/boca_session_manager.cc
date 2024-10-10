// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_manager.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_session_util.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/common/api_error_codes.h"

namespace ash::boca {

BocaSessionManager::BocaSessionManager(SessionClientImpl* session_client_impl,
                                       AccountId account_id)
    : account_id_(std::move(account_id)),
      session_client_impl_(std::move(session_client_impl)) {
  GetNetworkConfigService(cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(
      cros_network_config_observer_.BindNewPipeAndPassRemote());
  //  Register BocaSessionManager for the current profile.
  if (BocaAppClient::HasInstance()) {
    BocaAppClient::Get()->AddSessionManager(this);
  }
  LoadInitialNetworkState();
}
BocaSessionManager::~BocaSessionManager() = default;

void BocaSessionManager::Observer::OnBundleUpdated(
    const ::boca::Bundle& bundle) {}

void BocaSessionManager::Observer::OnSessionCaptionConfigUpdated(
    const std::string& group_name,
    const ::boca::CaptionsConfig& config) {}

void BocaSessionManager::Observer::OnLocalCaptionConfigUpdated(
    const ::boca::CaptionsConfig& config) {}

void BocaSessionManager::Observer::OnSessionRosterUpdated(
    const std::string& group_name,
    const std::vector<::boca::UserIdentity>& consumers) {}

void BocaSessionManager::Observer::OnAppReloaded() {}

void BocaSessionManager::Observer::OnConsumerActivityUpdated(
    const std::map<std::string, ::boca::StudentStatus>& activities) {}

void BocaSessionManager::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state) {
  // Check network types comment here:
  // chromeos/services/network_config/public/mojom/network_types.mojom
  if (chromeos::network_config::StateIsConnected(
          network_state->connection_state)) {
    if (!is_network_conntected_) {
      // Explicitly trigger a load whenever network back online. This will cover
      // the case for initial ctor too.
      // Other network change may trigger this events too, only handle when
      // flipped from offline to online.
      is_network_conntected_ = true;
      LoadCurrentSession();
    }
  } else {
    is_network_conntected_ = false;
  }
}

void BocaSessionManager::NotifyError(BocaError error) {}

void BocaSessionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BocaSessionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BocaSessionManager::StartSessionPolling() {
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, kPollingInterval, this,
                 &BocaSessionManager::LoadCurrentSession);
  }
}

void BocaSessionManager::LoadCurrentSession() {
  if (!is_network_conntected_) {
    return;
  }

  // TODO(b/361852484): We should ideally listen to user switch events. But
  // since we'll remove polling after we have FCM, leave it as it is now.
  if (!IsProfileActive()) {
    return;
  }
  auto request = std::make_unique<GetSessionRequest>(
      session_client_impl_->sender(), account_id_.GetGaiaId(),
      base::BindOnce(&BocaSessionManager::ParseSessionResponse,
                     weak_factory_.GetWeakPtr()));
  session_client_impl_->GetSession(std::move(request));
}

void BocaSessionManager::ParseSessionResponse(
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    return;
  }
  UpdateCurrentSession(std::move(result.value()), true);
}

void BocaSessionManager::UpdateCurrentSession(
    std::unique_ptr<::boca::Session> session,
    bool dispatch_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  previous_session_ = std::move(current_session_);
  current_session_ = std::move(session);
  if (dispatch_event) {
    NotifySessionUpdate();
    NotifyOnTaskUpdate();
    NotifyCaptionConfigUpdate();
    NotifyRosterUpdate();
    NotifyConsumerActivityUpdate();
  }
}

::boca::Session* BocaSessionManager::GetCurrentSession() {
  return current_session_.get();
}

void BocaSessionManager::UpdateTabActivity(std::u16string title) {
  if (!current_session_ ||
      current_session_->session_state() != ::boca::Session::ACTIVE) {
    return;
  }
  auto session_id = current_session_->session_id();
  auto gaia_id = account_id_.GetGaiaId();
  auto device_id = BocaAppClient::Get()->GetDeviceId();
  auto request = std::make_unique<UpdateStudentActivitiesRequest>(
      session_client_impl_->sender(), session_id, gaia_id,
      !device_id.empty() ? device_id : kDummyDeviceId,
      base::BindOnce(
          [](base::expected<bool, google_apis::ApiErrorCode> result) {
            if (result.has_value()) {
              // TODO(b/366316261):Add metrics for update failure.
              LOG(WARNING) << "[Boca]Failed to update student activity.";
            }
          }));
  request->set_active_tab_title(base::UTF16ToUTF8(title));
  session_client_impl_->UpdateStudentActivity(std::move(request));
}

void BocaSessionManager::NotifyLocalCaptionEvents(
    ::boca::CaptionsConfig caption_config) {
  for (auto& observer : observers_) {
    observer.OnLocalCaptionConfigUpdated(std::move(caption_config));
  }
}

void BocaSessionManager::NotifyAppReload() {
  for (auto& observer : observers_) {
    observer.OnAppReloaded();
  }
}

void BocaSessionManager::LoadInitialNetworkState() {
  cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kVisible,
          chromeos::network_config::mojom::NetworkType::kAll,
          /*limit=*/0),
      base::BindOnce(&BocaSessionManager::OnNetworkStateFetched,
                     weak_factory_.GetWeakPtr()));
}

void BocaSessionManager::OnNetworkStateFetched(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  for (const chromeos::network_config::mojom::NetworkStatePropertiesPtr&
           network : networks) {
    if (chromeos::network_config::StateIsConnected(network->connection_state)) {
      is_network_conntected_ = true;
      break;
    }
  }
}

bool BocaSessionManager::IsProfileActive() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->GetActiveUser() &&
         user_manager::UserManager::Get()->GetActiveUser()->GetAccountId() ==
             account_id_;
}

void BocaSessionManager::NotifySessionUpdate() {
  if ((!current_session_ ||
       current_session_->session_state() != ::boca::Session::ACTIVE) &&
      previous_session_ &&
      previous_session_->session_state() == ::boca::Session::ACTIVE) {
    for (auto& observer : observers_) {
      // Stop polling when session ends.
      if (timer_.IsRunning()) {
        timer_.Stop();
      }
      observer.OnSessionEnded(previous_session_->session_id());
    }
  }

  if (current_session_ &&
      current_session_->session_state() == ::boca::Session::ACTIVE &&
      (!previous_session_ ||
       previous_session_->session_state() != ::boca::Session::ACTIVE)) {
    for (auto& observer : observers_) {
      // Start polling after session start.
      if (!timer_.IsRunning()) {
        timer_.Start(FROM_HERE, kPollingInterval, this,
                     &BocaSessionManager::LoadCurrentSession);
      }
      observer.OnSessionStarted(current_session_->session_id(),
                                current_session_->teacher());
    }
  }
}

void BocaSessionManager::NotifyOnTaskUpdate() {
  auto previous_bundle = GetSessionConfigSafe(previous_session_.get())
                             .on_task_config()
                             .active_bundle();
  auto current_bundle = GetSessionConfigSafe(current_session_.get())
                            .on_task_config()
                            .active_bundle();
  if (previous_bundle.SerializeAsString() !=
      current_bundle.SerializeAsString()) {
    for (auto& observer : observers_) {
      observer.OnBundleUpdated(current_bundle);
    }
  }
}

void BocaSessionManager::NotifyCaptionConfigUpdate() {
  auto previous_session_caption_config =
      GetSessionConfigSafe(previous_session_.get()).captions_config();
  auto current_session_caption_config =
      GetSessionConfigSafe(current_session_.get()).captions_config();
  if (previous_session_caption_config.SerializeAsString() !=
      current_session_caption_config.SerializeAsString()) {
    for (auto& observer : observers_) {
      observer.OnSessionCaptionConfigUpdated(kMainStudentGroupName,
                                             current_session_caption_config);
    }
  }
}

void BocaSessionManager::NotifyRosterUpdate() {
  auto previous_session_roster = GetRosterSafe(previous_session_.get());
  auto current_session_roster = GetRosterSafe(current_session_.get());
  if (previous_session_roster.SerializeAsString() !=
      current_session_roster.SerializeAsString()) {
    for (auto& observer : observers_) {
      auto student_list = GetStudentGroupsSafe(current_session_.get());
      observer.OnSessionRosterUpdated(
          kMainStudentGroupName, std::vector<::boca::UserIdentity>(
                                     student_list.begin(), student_list.end()));
    }
  }
}

void BocaSessionManager::NotifyConsumerActivityUpdate() {
  if (!current_session_ || !previous_session_) {
    return;
  }
  auto current_activity = current_session_->student_statuses();
  auto previous_activity = previous_session_->student_statuses();
  for (auto status : current_activity) {
    auto key = status.first;
    if (!previous_activity.contains(key) ||
        (previous_activity.at(key).SerializeAsString() !=
         status.second.SerializeAsString())) {
      for (auto& observer : observers_) {
        observer.OnConsumerActivityUpdated(
            std::map<std::string, ::boca::StudentStatus>(
                current_activity.begin(), current_activity.end()));
      }
      return;
    }
  }
}

}  // namespace ash::boca
