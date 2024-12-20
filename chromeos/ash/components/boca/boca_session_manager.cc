// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_manager.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_util.h"
#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/message_center/message_center.h"

namespace ash::boca {

BocaSessionManager::BocaSessionManager(SessionClientImpl* session_client_impl,
                                       AccountId account_id,
                                       bool is_producer)
    : is_producer_(is_producer),
      account_id_(std::move(account_id)),
      session_client_impl_(std::move(session_client_impl)) {
  in_session_polling_interval_ =
      features::IsBocaCustomPollingEnabled()
          ? ash::features::kBocaInSessionPeriodicJobIntervalInSeconds.Get()
          : base::Seconds(kDefaultPollingIntervalInSeconds);
  indefinite_polling_interval_ =
      features::IsBocaCustomPollingEnabled()
          ? ash::features::kBocaIndefinitePeriodicJobIntervalInSeconds.Get()
          : base::Seconds(kDefaultPollingIntervalInSeconds);

  GetNetworkConfigService(cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(
      cros_network_config_observer_.BindNewPipeAndPassRemote());
  //  Register BocaSessionManager for the current profile.
  if (BocaAppClient::HasInstance()) {
    BocaAppClient::Get()->AddSessionManager(this);
    identity_manager_ = BocaAppClient::Get()->GetIdentityManager();
    if (identity_manager_) {
      identity_manager_->AddObserver(this);
    }
  }
  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager::Get()->AddSessionStateObserver(this);
  }
  LoadInitialNetworkState();
  LoadCurrentSession(/*from_polling=*/false);
  StartSessionPolling(/*in_session=*/false);
}

BocaSessionManager::~BocaSessionManager() {
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
  if (indefinite_timer_.IsRunning()) {
    indefinite_timer_.Stop();
  }
  if (user_manager::UserManager::IsInitialized()) {
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  }
}

void BocaSessionManager::Observer::OnBundleUpdated(
    const ::boca::Bundle& bundle) {}

void BocaSessionManager::Observer::OnSessionCaptionConfigUpdated(
    const std::string& group_name,
    const ::boca::CaptionsConfig& config,
    const std::string& tachyon_group_id) {}

void BocaSessionManager::Observer::OnLocalCaptionConfigUpdated(
    const ::boca::CaptionsConfig& config) {}

void BocaSessionManager::Observer::OnSessionRosterUpdated(
    const ::boca::Roster& roster) {}

void BocaSessionManager::Observer::OnAppReloaded() {}

void BocaSessionManager::Observer::OnConsumerActivityUpdated(
    const std::map<std::string, ::boca::StudentStatus>& activities) {}

void BocaSessionManager::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state) {
  // Check network types comment here:
  // chromeos/services/network_config/public/mojom/network_types.mojom
  if (chromeos::network_config::StateIsConnected(
          network_state->connection_state)) {
    if (!is_network_connected_) {
      // Explicitly trigger a load whenever network back online. This will cover
      // the case for initial ctor too.
      // Other network change may trigger this events too, only handle when
      // flipped from offline to online.
      is_network_connected_ = true;
      LoadCurrentSession(/*from_polling=*/false);
    }
  } else {
    is_network_connected_ = false;
  }
}

void BocaSessionManager::NotifyError(BocaError error) {}

void BocaSessionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BocaSessionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BocaSessionManager::StartSessionPolling(bool in_session) {
  if (in_session) {
    if (in_session_polling_interval_ == base::Seconds(0)) {
      return;
    }
    if (indefinite_timer_.IsRunning()) {
      indefinite_timer_.Stop();
    }
    if (!in_session_timer_.IsRunning()) {
      in_session_timer_.Start(FROM_HERE, in_session_polling_interval_, this,
                              &BocaSessionManager::MaybeLoadCurrentSession);
    }
  } else {
    if (indefinite_polling_interval_ == base::Seconds(0)) {
      return;
    }
    if (in_session_timer_.IsRunning()) {
      in_session_timer_.Stop();
    }
    if (!indefinite_timer_.IsRunning()) {
      indefinite_timer_.Start(FROM_HERE, indefinite_polling_interval_, this,
                              &BocaSessionManager::MaybeLoadCurrentSession);
    }
  }
}

void BocaSessionManager::MaybeLoadCurrentSession() {
  // Only skip session load for scheduled polling if there is any load since
  // last schedule, we should never skip it for invalidation.
  if (base::TimeTicks::Now() - last_session_load_ <
      in_session_polling_interval_) {
    return;
  }
  LoadCurrentSession(/*from_polling=*/true);
}

void BocaSessionManager::LoadCurrentSession(bool from_polling) {
  // TODO(crbug.com/374788934): Currently always try fetching regardless of
  // network status as we've seen inconsistent behavior between machines
  // regarding network config, revisit this.

  // TODO(b/361852484): We should ideally listen to user switch events. But
  // since we'll remove polling after we have FCM, leave it as it is now.
  if (!IsProfileActive()) {
    return;
  }
  auto request = std::make_unique<GetSessionRequest>(
      session_client_impl_->sender(), is_producer_, account_id_.GetGaiaId(),
      base::BindOnce(&BocaSessionManager::ParseSessionResponse,
                     weak_factory_.GetWeakPtr(), from_polling));
  session_client_impl_->GetSession(std::move(request));
}

void BocaSessionManager::ParseSessionResponse(
    bool from_polling,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    return;
  }

  if (from_polling) {
    RecordPollingResult(current_session_.get(), result.value().get());
  }

  UpdateCurrentSession(std::move(result.value()), true);
}

void BocaSessionManager::UpdateCurrentSession(
    std::unique_ptr<::boca::Session> session,
    bool dispatch_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsSessionTakeOver(current_session_.get(), session.get())) {
    HandleTakeOver(dispatch_event, std::move(session));
    return;
  }
  previous_session_ = std::move(current_session_);
  current_session_ = std::move(session);
  last_session_load_ = base::TimeTicks::Now();

  if (dispatch_event) {
    DispatchEvent();
  }
}

::boca::Session* BocaSessionManager::GetCurrentSession() {
  return current_session_.get();
}

const ::boca::Session* BocaSessionManager::GetPreviousSession() {
  return previous_session_.get();
}

void BocaSessionManager::UpdateTabActivity(std::u16string title) {
  if (!current_session_ ||
      current_session_->session_state() != ::boca::Session::ACTIVE) {
    return;
  }
  if (title == active_tab_title_) {
    return;
  }
  active_tab_title_ = std::move(title);
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

  // TODO(crbug.com/376550427):Make a permanet fix to provide URL resource for
  // home page, and remove this after that.
  request->set_active_tab_title(active_tab_title_.empty()
                                    ? kHomePageTitle
                                    : base::UTF16ToUTF8(active_tab_title_));
  session_client_impl_->UpdateStudentActivity(std::move(request));
}

void BocaSessionManager::ToggleAppStatus(bool is_app_opened) {
  is_app_opened_ = is_app_opened;
}

void BocaSessionManager::NotifyLocalCaptionEvents(
    ::boca::CaptionsConfig caption_config) {
  for (auto& observer : observers_) {
    observer.OnLocalCaptionConfigUpdated(std::move(caption_config));
  }
  is_local_caption_enabled_ = caption_config.captions_enabled();
  if (is_producer_) {
    notification_handler_.HandleCaptionNotification(
        message_center::MessageCenter::Get(), is_local_caption_enabled_,
        GetSessionConfigSafe(current_session_.get())
            .captions_config()
            .captions_enabled());
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
      is_network_connected_ = true;
      break;
    }
  }
}

void BocaSessionManager::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& info) {
  if (info.email != account_id_.GetUserEmail()) {
    return;
  }
  LoadCurrentSession(/*from_polling=*/false);
}

void BocaSessionManager::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
}

void BocaSessionManager::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user || active_user->GetAccountId() != account_id_) {
    return;
  }
  LoadCurrentSession(/*from_polling=*/false);
}

bool BocaSessionManager::IsProfileActive() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->GetActiveUser() &&
         user_manager::UserManager::Get()->GetActiveUser()->GetAccountId() ==
             account_id_;
}

bool BocaSessionManager::IsSessionActive(const ::boca::Session* session) {
  return session && session->session_state() == ::boca::Session::ACTIVE &&
         !session->session_id().empty();
}

bool BocaSessionManager::IsSessionTakeOver(
    const ::boca::Session* previous_session,
    const ::boca::Session* current_session) {
  if (!IsSessionActive(previous_session) || !IsSessionActive(current_session)) {
    return false;
  }
  return previous_session->session_id() != current_session->session_id();
}

void BocaSessionManager::RecordPollingResult(
    const ::boca::Session* previous_session,
    const ::boca::Session* current_session) {
  BocaPollingResult polling_result;
  if (!previous_session && !current_session) {
    polling_result = BocaPollingResult::kNoUpdate;
  } else if (!previous_session) {
    polling_result = BocaPollingResult::kSessionStart;
  } else if (!current_session) {
    polling_result = BocaPollingResult::kSessionEnd;
  } else if (previous_session->SerializeAsString() !=
             current_session->SerializeAsString()) {
    polling_result = BocaPollingResult::kInSessionUpdate;
  } else {
    polling_result = BocaPollingResult::kNoUpdate;
  }
  base::UmaHistogramEnumeration(kPollingResultHistName, polling_result);
}

void BocaSessionManager::HandleTakeOver(
    bool dispatch_event,
    std::unique_ptr<::boca::Session> session) {
  previous_session_ = std::move(current_session_);
  current_session_ = nullptr;
  if (dispatch_event) {
    DispatchEvent();
  }
  previous_session_ = nullptr;
  current_session_ = std::move(session);
  if (dispatch_event) {
    DispatchEvent();
  }
}

void BocaSessionManager::DispatchEvent() {
  NotifySessionUpdate();
  NotifyOnTaskUpdate();
  NotifySessionCaptionConfigUpdate();
  NotifyRosterUpdate();
  NotifyConsumerActivityUpdate();
}

void BocaSessionManager::NotifySessionUpdate() {
  if (IsSessionActive(previous_session_.get()) &&
      !IsSessionActive(current_session_.get())) {
    for (auto& observer : observers_) {
      StartSessionPolling(/*in_session=*/false);
      observer.OnSessionEnded(previous_session_->session_id());
      if (is_producer_) {
        notification_handler_.HandleSessionEndedNotification(
            message_center::MessageCenter::Get());
      }
    }
  }

  if (!IsSessionActive(previous_session_.get()) &&
      IsSessionActive(current_session_.get())) {
    for (auto& observer : observers_) {
      StartSessionPolling(/*in_session=*/true);
      observer.OnSessionStarted(current_session_->session_id(),
                                current_session_->teacher());
      if (is_producer_) {
        notification_handler_.HandleSessionStartedNotification(
            message_center::MessageCenter::Get());
      }
    }
  }
}

void BocaSessionManager::NotifyOnTaskUpdate() {
  if (!IsSessionActive(current_session_.get())) {
    return;
  }
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

void BocaSessionManager::NotifySessionCaptionConfigUpdate() {
  if (!IsSessionActive(current_session_.get())) {
    return;
  }

  auto current_session_caption_config =
      GetSessionConfigSafe(current_session_.get()).captions_config();

  // We should never turn on caption for teacher when app is not opened. We
  // already make sure turn off caption when app load/unload, but in the event
  // of OS crash, we won't be able to fire update in time, this would cause
  // server caption config to be still on. This check make sure we don't turn on
  // it for user before they realize.
  if (is_producer_ && !is_app_opened_ &&
      current_session_caption_config.captions_enabled()) {
    return;
  }

  auto previous_session_caption_config =
      GetSessionConfigSafe(previous_session_.get()).captions_config();

  if (previous_session_caption_config.SerializeAsString() !=
      current_session_caption_config.SerializeAsString()) {
    for (auto& observer : observers_) {
      observer.OnSessionCaptionConfigUpdated(
          kMainStudentGroupName, current_session_caption_config,
          // TODO(crbug.com/374200256):Should not notify events when session
          // ended. Remove the null check after that.
          current_session_ ? current_session_->tachyon_group_id()
                           : std::string());
    }
    if (is_producer_) {
      notification_handler_.HandleCaptionNotification(
          message_center::MessageCenter::Get(), is_local_caption_enabled_,
          current_session_caption_config.captions_enabled());
    }
  }
}

void BocaSessionManager::NotifyRosterUpdate() {
  if (!IsSessionActive(current_session_.get())) {
    return;
  }
  auto previous_session_roster = GetRosterSafe(previous_session_.get());
  auto current_session_roster = GetRosterSafe(current_session_.get());
  if (previous_session_roster.SerializeAsString() !=
      current_session_roster.SerializeAsString()) {
    for (auto& observer : observers_) {
      observer.OnSessionRosterUpdated(current_session_roster);
    }
  }
}

void BocaSessionManager::NotifyConsumerActivityUpdate() {
  if (!IsSessionActive(current_session_.get()) || !previous_session_) {
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
