// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_manager.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_util.h"
#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/student_heartbeat_request.h"
#include "chromeos/ash/components/boca/session_api/update_student_activities_request.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash::boca {

namespace {
const net::BackoffEntry::Policy kStudentHeartbeatBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Seconds(30).InMilliseconds(),
    .multiply_factor = 1.2,
    .jitter_factor = 0.2,
    .maximum_backoff_ms = base::Seconds(90).InMilliseconds(),
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false};
}  // namespace

BocaSessionManager::BocaSessionManager(
    SessionClientImpl* session_client_impl,
    const PrefService* pref_service,
    AccountId account_id,
    bool is_producer,
    std::unique_ptr<SpotlightRemotingClientManager> remoting_client_manager)
    : is_producer_(is_producer),
      account_id_(std::move(account_id)),
      remoting_client_manager_(std::move(remoting_client_manager)),
      pref_service_(pref_service),
      session_client_impl_(std::move(session_client_impl)),
      student_heartbeat_retry_backoff_{&kStudentHeartbeatBackoffPolicy} {
  in_session_polling_interval_ =
      features::IsBocaCustomPollingEnabled()
          ? ash::features::kBocaInSessionPeriodicJobIntervalInSeconds.Get()
          : base::Seconds(kDefaultPollingIntervalInSeconds);
  indefinite_polling_interval_ =
      features::IsBocaCustomPollingEnabled()
          ? ash::features::kBocaIndefinitePeriodicJobIntervalInSeconds.Get()
          : base::Seconds(kDefaultPollingIntervalInSeconds);
  student_heartbeat_interval_ =
      features::IsBocaStudentHeartbeatCustomIntervalEnabled()
          ? ash::features::kBocaStudentHeartbeatPeriodicJobIntervalInSeconds
                .Get()
          : base::Seconds(kDefaultStudentHeartbeatIntervalInSeconds);

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
  if (session_manager::SessionManager::Get()) {
    session_manager_observation_.Observe(
        session_manager::SessionManager::Get());
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

void BocaSessionManager::Observer::OnSessionMetadataUpdated(
    const std::string& session_id) {}

void BocaSessionManager::Observer::OnBundleUpdated(
    const ::boca::Bundle& bundle) {}

void BocaSessionManager::Observer::OnSessionCaptionConfigUpdated(
    const std::string& group_name,
    const ::boca::CaptionsConfig& config,
    const std::string& tachyon_group_id) {}

void BocaSessionManager::Observer::OnLocalCaptionConfigUpdated(
    const ::boca::CaptionsConfig& config) {}

void BocaSessionManager::Observer::OnSodaStatusUpdate(SodaStatus status) {}

void BocaSessionManager::Observer::OnLocalCaptionClosed() {}

void BocaSessionManager::Observer::OnSessionCaptionClosed(bool is_error) {}

void BocaSessionManager::Observer::OnSessionRosterUpdated(
    const ::boca::Roster& roster) {}

void BocaSessionManager::Observer::OnAppReloaded() {}

void BocaSessionManager::Observer::OnConsumerActivityUpdated(
    const std::map<std::string, ::boca::StudentStatus>& activities) {}

void BocaSessionManager::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check network types comment here:
  // chromeos/services/network_config/public/mojom/network_types.mojom
  if (chromeos::network_config::StateIsConnected(
          network_state->connection_state)) {
    // Update network restriction before load session.
    UpdateNetworkRestriction(std::move(network_state));
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
      (in_session_polling_interval_ -
       base::Seconds(kSkipPollingBufferInSeconds))) {
    return;
  }
  LoadCurrentSession(/*from_polling=*/true);
}

void BocaSessionManager::LoadCurrentSession(bool from_polling) {
  // TODO: crbug.com/374788934 - Currently always try fetching regardless of
  // network status as we've seen inconsistent behavior between machines
  // regarding network config, revisit this.

  // TODO: crbug.com/361852484 - We should ideally listen to user switch events.
  // But since we'll remove polling after we have FCM, leave it as it is now.
  if (!IsProfileActive()) {
    return;
  }
  if (disabled_on_non_managed_network_) {
    UpdateCurrentSession(std::unique_ptr<::boca::Session>(nullptr),
                         /*dispatch_event=*/true);
    return;
  }

  auto request = std::make_unique<GetSessionRequest>(
      session_client_impl_->sender(),
      BocaAppClient::Get()->GetSchoolToolsServerBaseUrl(), is_producer_,
      account_id_.GetGaiaId(),
      base::BindOnce(&BocaSessionManager::ParseSessionResponse,
                     weak_factory_.GetWeakPtr(), from_polling));
  request->set_device_id(BocaAppClient::Get()->GetDeviceId());
  session_client_impl_->GetSession(std::move(request),
                                   /*can_skip_duplicate_request=*/true);
}

void BocaSessionManager::ParseSessionResponse(
    bool from_polling,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    return;
  }

  if (from_polling) {
    boca::RecordPollingResult(current_session_.get(), result.value().get());
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

  last_session_load_ = base::TimeTicks::Now();
  HandleSessionUpdate(std::move(current_session_), std::move(session),
                      /*dispatch_event=*/true);
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
      session_client_impl_->sender(),
      BocaAppClient::Get()->GetSchoolToolsServerBaseUrl(), session_id, gaia_id,
      !device_id.empty() ? device_id : kDummyDeviceId,
      base::BindOnce(
          [](base::expected<bool, google_apis::ApiErrorCode> result) {
            if (!result.has_value()) {
              boca::RecordUpdateStudentActivitiesErrorCode(result.error());
              LOG(WARNING)
                  << "[Boca]Failed to update student activity with error code: "
                  << result.error();
            }
          }));

  // TODO: crbug.com/376550427 - Make a permanet fix to provide URL resource for
  // home page, and remove this after that.
  request->set_active_tab_title(
      active_tab_title_.empty()
          ? l10n_util::GetStringUTF8(IDS_CLASS_TOOLS_HOME_PAGE)
          : base::UTF16ToUTF8(active_tab_title_));
  session_client_impl_->UpdateStudentActivity(std::move(request));
}

void BocaSessionManager::OnAppWindowOpened() {
  if (soda_installer_ != nullptr) {
    // TODO(378702821) Notify observers of SODA status change.
    soda_installer_->InstallSoda(
        base::BindOnce(&BocaSessionManager::NotifySodaStatusListeners,
                       weak_factory_.GetWeakPtr()));
  }
}

void BocaSessionManager::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->IsScreenLocked()) {
    CloseAllCaptions();
  }
}

void BocaSessionManager::NotifyLocalCaptionEvents(
    ::boca::CaptionsConfig caption_config) {
  for (auto& observer : observers_) {
    observer.OnLocalCaptionConfigUpdated(caption_config);
  }
  is_local_caption_enabled_ = caption_config.captions_enabled();
  HandleCaptionNotification();
}

void BocaSessionManager::NotifyLocalCaptionClosed() {
  for (auto& observer : observers_) {
    observer.OnLocalCaptionClosed();
  }
  is_local_caption_enabled_ = false;
  HandleCaptionNotification();
}

void BocaSessionManager::NotifySessionCaptionProducerEvents(
    const ::boca::CaptionsConfig& caption_config) {
  if (!is_producer_ || !IsSessionActive(current_session_.get())) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnSessionCaptionConfigUpdated(
        kMainStudentGroupName, caption_config,
        current_session_->tachyon_group_id());
  }
}

void BocaSessionManager::NotifyAppReload() {
  for (auto& observer : observers_) {
    observer.OnAppReloaded();
  }
}

bool BocaSessionManager::disabled_on_non_managed_network() {
  return disabled_on_non_managed_network_;
}

void BocaSessionManager::SetSessionCaptionInitializer(
    SessionCaptionInitializer session_caption_initializer) {
  session_caption_initializer_ = session_caption_initializer;
}

void BocaSessionManager::RemoveSessionCaptionInitializer() {
  session_caption_initializer_.Reset();
}

void BocaSessionManager::InitSessionCaption(
    base::OnceCallback<void(bool)> success_cb) {
  if (!session_caption_initializer_) {
    // Initializer not set so nothing to do.
    std::move(success_cb).Run(true);
    return;
  }
  session_caption_initializer_.Run(std::move(success_cb));
}

BocaSessionManager::SodaStatus BocaSessionManager::GetSodaStatus() {
  if (soda_installer_ != nullptr) {
    return soda_installer_->GetStatus();
  }

  return SodaStatus::kUninstalled;
}

void BocaSessionManager::StartCrdClient(
    std::string crd_connection_code,
    base::OnceClosure done_callback,
    SpotlightFrameConsumer::FrameReceivedCallback frame_received_callback,
    SpotlightCrdStateUpdatedCallback crd_state_callback) {
  CHECK(ash::features::IsBocaSpotlightRobotRequesterEnabled());

  remoting_client_manager_->StartCrdClient(
      crd_connection_code, std::move(done_callback),
      std::move(frame_received_callback), std::move(crd_state_callback));
}

void BocaSessionManager::EndSpotlightSession() {
  CHECK(ash::features::IsBocaSpotlightRobotRequesterEnabled());
  remoting_client_manager_->StopCrdClient();
}

std::string BocaSessionManager::GetDeviceRobotEmail() {
  CHECK(ash::features::IsBocaSpotlightRobotRequesterEnabled());
  return remoting_client_manager_->GetDeviceRobotEmail();
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (chromeos::network_config::mojom::NetworkStatePropertiesPtr& network :
       networks) {
    if (chromeos::network_config::StateIsConnected(network->connection_state)) {
      is_network_connected_ = true;
      UpdateNetworkRestriction(std::move(network));
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
    CloseAllCaptions();
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

void BocaSessionManager::HandleTakeOver(
    bool dispatch_event,
    std::unique_ptr<::boca::Session> session) {
  HandleSessionUpdate(std::move(current_session_), nullptr,
                      /*dispatch_event=*/true);
  HandleSessionUpdate(nullptr, std::move(session), /*dispatch_event=*/true);
}

void BocaSessionManager::DispatchEvent() {
  NotifySessionUpdate();
  NotifySessionMetadataUpdate();
  NotifyOnTaskUpdate();
  NotifySessionCaptionConfigUpdate();
  NotifyRosterUpdate();
  NotifyConsumerActivityUpdate();
}

void BocaSessionManager::NotifySessionUpdate() {
  if (IsSessionActive(previous_session_.get()) &&
      !IsSessionActive(current_session_.get())) {
    for (auto& observer : observers_) {
      VLOG(1) << "[Boca] notifying session ended";
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
      VLOG(1) << "[Boca] notifying session started";
      StartSessionPolling(/*in_session=*/true);
      observer.OnSessionStarted(current_session_->session_id(),
                                current_session_->teacher());
      if (is_producer_) {
        notification_handler_.HandleSessionStartedNotification(
            message_center::MessageCenter::Get());
      }
    }
  }

  if (IsSessionActive(current_session_.get())) {
    StartSendingStudentHeartbeatRequests();
  } else {
    StopSendingStudentHeartbeatRequests();
  }
}

void BocaSessionManager::NotifySessionMetadataUpdate() {
  if (!IsSessionActive(current_session_.get()) ||
      !IsSessionActive(previous_session_.get())) {
    return;
  }
  if (current_session_->teacher().SerializeAsString() !=
          previous_session_->teacher().SerializeAsString() ||
      current_session_->duration().SerializeAsString() !=
          previous_session_->duration().SerializeAsString()) {
    for (auto& observer : observers_) {
      observer.OnSessionMetadataUpdated(current_session_->session_id());
    }
    return;
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
  // Session captions notifications for producer is done by calling
  // `NotifySessionCaptionProducerEvents`
  if (is_producer_) {
    return;
  }
  if (!IsSessionActive(current_session_.get())) {
    VLOG(1) << "[Boca] no active session, will not notify captions update";
    return;
  }

  auto current_session_caption_config =
      GetSessionConfigSafe(current_session_.get()).captions_config();

  auto previous_session_caption_config =
      GetSessionConfigSafe(previous_session_.get()).captions_config();

  if (previous_session_caption_config.SerializeAsString() !=
      current_session_caption_config.SerializeAsString()) {
    VLOG(1) << "[Boca] notify captions update";
    for (auto& observer : observers_) {
      observer.OnSessionCaptionConfigUpdated(
          kMainStudentGroupName, current_session_caption_config,
          // TODO: crbug.com/374200256 - Should not notify events when session
          // ended. Remove the null check after that.
          current_session_ ? current_session_->tachyon_group_id()
                           : std::string());
    }
    HandleCaptionNotification();
  } else {
    VLOG(1) << "[Boca] no captions change, will not notify. Captions enabled: "
            << current_session_caption_config.captions_enabled()
            << ", translation enabled: "
            << current_session_caption_config.translations_enabled();
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
  if (!IsSessionActive(current_session_.get())) {
    return;
  }

  auto current_activity = current_session_->student_statuses();

  if (!previous_session_ && current_activity.empty()) {
    return;
  }

  if (!previous_session_) {
    for (auto& observer : observers_) {
      observer.OnConsumerActivityUpdated(
          std::map<std::string, ::boca::StudentStatus>(current_activity.begin(),
                                                       current_activity.end()));
    }
    return;
  }

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

void BocaSessionManager::HandleSessionUpdate(
    std::unique_ptr<::boca::Session> previous_session,
    std::unique_ptr<::boca::Session> current_session,
    bool dispatch_event) {
  previous_session_ = std::move(previous_session);
  current_session_ = std::move(current_session);
  UpdateLocalSessionDurationTracker();
  if (dispatch_event) {
    DispatchEvent();
  }
}

void BocaSessionManager::UpdateLocalSessionDurationTracker() {
  // Update timer to track session duration on client side.
  if (IsSessionActive(current_session_.get())) {
    if (!IsSessionActive(previous_session_.get())) {
      const auto nanos = current_session_->start_time().nanos();
      const auto seconds = current_session_->start_time().seconds();
      last_session_start_time_ = base::Time::FromSecondsSinceUnixEpoch(
          seconds +
          static_cast<double>(nanos) / base::Time::kNanosecondsPerSecond);
    }
    base::TimeDelta current_session_duration =
        base::Seconds(current_session_.get()->duration().seconds());
    // Update session duration to 0 should never happen, this is just sanity
    // check to ensure backwards compatibility.
    if (current_session_duration != base::Seconds(0) &&
        current_session_duration != last_session_duration_) {
      last_session_duration_ = current_session_duration;
      base::TimeDelta session_remaining =
          last_session_start_time_ + last_session_duration_ - base::Time::Now();
      session_duration_timer_.Start(
          // Add buffer to account for device drift. It's ok if we slightly
          // delay sign out after session end when network is lost.
          FROM_HERE,
          session_remaining +
              base::Seconds(kLocalSessionTrackerBufferInSeconds),
          base::BindOnce(&BocaSessionManager::UpdateCurrentSession,
                         base::Unretained(this), /*session=*/nullptr,
                         /*dispatch_event=*/true));
    }
  } else {
    session_duration_timer_.Stop();
    last_session_duration_ = base::Seconds(0);
  }
}

void BocaSessionManager::HandleCaptionNotification() {
  if (!is_producer_) {
    return;
  }
  notification_handler_.HandleCaptionNotification(
      message_center::MessageCenter::Get(), is_local_caption_enabled_,
      GetSessionConfigSafe(current_session_.get())
          .captions_config()
          .captions_enabled());
}

void BocaSessionManager::StartSendingStudentHeartbeatRequests() {
  if (!features::IsBocaStudentHeartbeatEnabled() || is_producer_ ||
      student_heartbeat_interval_ == base::Seconds(0)) {
    return;
  }
  if (!student_heartbeat_timer_.IsRunning() &&
      !student_heartbeat_backoff_timer_.IsRunning()) {
    student_heartbeat_timer_.Start(
        FROM_HERE, student_heartbeat_interval_, this,
        &BocaSessionManager::SendStudentHeartbeatRequest);
  }
}

void BocaSessionManager::StopSendingStudentHeartbeatRequests() {
  if (student_heartbeat_timer_.IsRunning()) {
    student_heartbeat_timer_.Stop();
  }
}

void BocaSessionManager::SendStudentHeartbeatRequest() {
  const std::string& session_id = current_session_->session_id();
  const GaiaId& gaia_id = account_id_.GetGaiaId();
  const std::string& device_id = BocaAppClient::Get()->GetDeviceId();
  const std::string& student_group_id =
      GetStudentGroupIdSafe(current_session_.get());
  auto request = std::make_unique<StudentHeartbeatRequest>(
      session_client_impl_->sender(),
      BocaAppClient::Get()->GetSchoolToolsServerBaseUrl(), session_id, gaia_id,
      device_id, student_group_id,
      base::BindOnce(&BocaSessionManager::OnStudentHeartbeat,
                     weak_factory_.GetWeakPtr()));
  session_client_impl_->StudentHeartbeat(std::move(request));
}

void BocaSessionManager::OnStudentHeartbeat(
    base::expected<bool, google_apis::ApiErrorCode> result) {
  if (!result.has_value()) {
    boca::RecordStudentHeartBeatErrorCode(result.error());
    LOG(WARNING) << "[Boca]Failed to call student heartbeat with error code: "
                 << result.error();
    if ((result.error() >= 500 && result.error() < 600) ||
        result.error() == 429) {
      student_heartbeat_retry_backoff_.InformOfRequest(/*succeeded=*/false);
      // Stop the repeating student heartbeat timer and start the backoff
      // oneshot timer.
      StopSendingStudentHeartbeatRequests();
      student_heartbeat_backoff_timer_.Start(
          FROM_HERE, student_heartbeat_retry_backoff_.GetTimeUntilRelease(),
          this, &BocaSessionManager::SendStudentHeartbeatRequest);
    }
    return;
  }
  student_heartbeat_retry_backoff_.Reset();
  StartSendingStudentHeartbeatRequests();
}

void BocaSessionManager::UpdateNetworkRestriction(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state) {
  bool should_disable_on_non_managed_network =
      (features::IsBocaNetworkRestrictionEnabled() ||
       (pref_service_->FindPreference(
            prefs::kClassManagementToolsNetworkRestrictionSetting) &&
        pref_service_->GetBoolean(
            prefs::kClassManagementToolsNetworkRestrictionSetting))) &&
      network_state->source !=
          chromeos::network_config::mojom::OncSource::kUserPolicy &&
      network_state->source !=
          chromeos::network_config::mojom::OncSource::kDevicePolicy;

  if (should_disable_on_non_managed_network !=
      disabled_on_non_managed_network_) {
    disabled_on_non_managed_network_ = should_disable_on_non_managed_network;
    LoadCurrentSession(/*from_polling=*/false);
  }
}

void BocaSessionManager::NotifySodaStatusListeners(SodaStatus status) {
  for (auto& observer : observers_) {
    observer.OnSodaStatusUpdate(status);
  }
}

void BocaSessionManager::CloseAllCaptions() {
  is_local_caption_enabled_ = false;
  for (auto& observer : observers_) {
    if (is_producer_) {
      observer.OnSessionCaptionClosed(/*is_error=*/false);
    }
    observer.OnLocalCaptionClosed();
  }
}

}  // namespace ash::boca
