// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_client.h"

#include <stddef.h>

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/base/encryptor.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "net/base/ip_endpoint.h"

namespace gcm {

// static
std::string FakeGCMClient::GenerateGCMRegistrationID(
    const std::vector<std::string>& sender_ids) {
  // GCMService normalizes the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  // Simulate the registration_id by concaternating all sender IDs.
  // Set registration_id to empty to denote an error if sender_ids contains a
  // hint.
  std::string registration_id;
  if (sender_ids.size() != 1 ||
      sender_ids[0].find("error") == std::string::npos) {
    for (size_t i = 0; i < normalized_sender_ids.size(); ++i) {
      if (i > 0)
        registration_id += ",";
      registration_id += normalized_sender_ids[i];
    }
  }
  return registration_id;
}

// static
std::string FakeGCMClient::GenerateInstanceIDToken(
    const std::string& authorized_entity, const std::string& scope) {
  if (authorized_entity.find("error") != std::string::npos)
    return "";
  std::string token(authorized_entity);
  token += ",";
  token += scope;
  return token;
}

FakeGCMClient::FakeGCMClient(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread)
    : delegate_(nullptr),
      started_(false),
      start_mode_(DELAYED_START),
      start_mode_overridding_(RESPECT_START_MODE),
      ui_thread_(ui_thread),
      io_thread_(io_thread) {}

FakeGCMClient::~FakeGCMClient() {
}

void FakeGCMClient::Initialize(
    const ChromeBuildInfo& chrome_build_info,
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<Encryptor> encryptor,
    Delegate* delegate) {
  product_category_for_subtypes_ =
      chrome_build_info.product_category_for_subtypes;
  delegate_ = delegate;
}

void FakeGCMClient::Start(StartMode start_mode) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  if (started_)
    return;

  if (start_mode == IMMEDIATE_START)
    start_mode_ = IMMEDIATE_START;
  if (start_mode_ == DELAYED_START ||
      start_mode_overridding_ == FORCE_TO_ALWAYS_DELAY_START_GCM) {
    return;
  }

  DoStart();
}

void FakeGCMClient::DoStart() {
  started_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeGCMClient::Started, weak_ptr_factory_.GetWeakPtr()));
}

void FakeGCMClient::Stop() {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());
  started_ = false;
  delegate_->OnDisconnected();
}

void FakeGCMClient::Register(
    scoped_refptr<RegistrationInfo> registration_info) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  std::string registration_id;

  GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    registration_id = GenerateGCMRegistrationID(
        gcm_registration_info->sender_ids);
  }

  InstanceIDTokenInfo* instance_id_token_info =
      InstanceIDTokenInfo::FromRegistrationInfo(registration_info.get());
  if (instance_id_token_info) {
    registration_id = GenerateInstanceIDToken(
        instance_id_token_info->authorized_entity,
        instance_id_token_info->scope);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeGCMClient::RegisterFinished,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(registration_info), registration_id));
}

bool FakeGCMClient::ValidateRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id) {
  return true;
}

void FakeGCMClient::Unregister(
    scoped_refptr<RegistrationInfo> registration_info) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeGCMClient::UnregisterFinished,
                     weak_ptr_factory_.GetWeakPtr(), registration_info));
}

void FakeGCMClient::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const OutgoingMessage& message) {
  DCHECK(io_thread_->RunsTasksInCurrentSequence());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeGCMClient::SendFinished,
                     weak_ptr_factory_.GetWeakPtr(), app_id, message));
}

void FakeGCMClient::RecordDecryptionFailure(const std::string& app_id,
                                            GCMDecryptionResult result) {
  recorder_.RecordDecryptionFailure(app_id, result);
}

void FakeGCMClient::SetRecording(bool recording) {
  recorder_.set_is_recording(recording);
}

void FakeGCMClient::ClearActivityLogs() {
  recorder_.Clear();
}

GCMClient::GCMStatistics FakeGCMClient::GetStatistics() const {
  GCMClient::GCMStatistics statistics;
  statistics.is_recording = recorder_.is_recording();

  recorder_.CollectActivities(&statistics.recorded_activities);
  return statistics;
}

void FakeGCMClient::SetAccountTokens(
    const std::vector<AccountTokenInfo>& account_tokens) {
}

void FakeGCMClient::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
}

void FakeGCMClient::RemoveAccountMapping(const CoreAccountId& account_id) {}

void FakeGCMClient::SetLastTokenFetchTime(const base::Time& time) {
}

void FakeGCMClient::UpdateHeartbeatTimer(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {}

void FakeGCMClient::AddInstanceIDData(const std::string& app_id,
                                      const std::string& instance_id,
                                      const std::string& extra_data) {
  instance_id_data_[app_id] = make_pair(instance_id, extra_data);
}

void FakeGCMClient::RemoveInstanceIDData(const std::string& app_id) {
  instance_id_data_.erase(app_id);
}

void FakeGCMClient::GetInstanceIDData(const std::string& app_id,
                                      std::string* instance_id,
                                      std::string* extra_data) {
  auto iter = instance_id_data_.find(app_id);
  if (iter == instance_id_data_.end()) {
    instance_id->clear();
    extra_data->clear();
    return;
  }

  *instance_id = iter->second.first;
  *extra_data = iter->second.second;
}

void FakeGCMClient::AddHeartbeatInterval(const std::string& scope,
                                         int interval_ms) {
}

void FakeGCMClient::RemoveHeartbeatInterval(const std::string& scope) {
}

void FakeGCMClient::PerformDelayedStart() {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeGCMClient::DoStart, weak_ptr_factory_.GetWeakPtr()));
}

void FakeGCMClient::ReceiveMessage(const std::string& app_id,
                                   const IncomingMessage& message) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeGCMClient::MessageReceived,
                     weak_ptr_factory_.GetWeakPtr(), app_id, message));
}

void FakeGCMClient::DeleteMessages(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksInCurrentSequence());

  io_thread_->PostTask(FROM_HERE,
                       base::BindOnce(&FakeGCMClient::MessagesDeleted,
                                      weak_ptr_factory_.GetWeakPtr(), app_id));
}

void FakeGCMClient::Started() {
  delegate_->OnGCMReady(std::vector<AccountMapping>(), base::Time());
  delegate_->OnConnected(net::IPEndPoint());
}

void FakeGCMClient::RegisterFinished(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registrion_id) {
  delegate_->OnRegisterFinished(std::move(registration_info), registrion_id,
                                registrion_id.empty() ? SERVER_ERROR : SUCCESS);
}

void FakeGCMClient::UnregisterFinished(
    scoped_refptr<RegistrationInfo> registration_info) {
  delegate_->OnUnregisterFinished(std::move(registration_info),
                                  GCMClient::SUCCESS);
}

void FakeGCMClient::SendFinished(const std::string& app_id,
                                 const OutgoingMessage& message) {
  delegate_->OnSendFinished(app_id, message.id, SUCCESS);

  // Simulate send error if message id contains a hint.
  if (message.id.find("error") != std::string::npos) {
    SendErrorDetails send_error_details;
    send_error_details.message_id = message.id;
    send_error_details.result = NETWORK_ERROR;
    send_error_details.additional_data = message.data;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeGCMClient::MessageSendError,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       send_error_details),
        base::Milliseconds(200));
  } else if(message.id.find("ack") != std::string::npos) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeGCMClient::SendAcknowledgement,
                       weak_ptr_factory_.GetWeakPtr(), app_id, message.id),
        base::Milliseconds(200));
  }
}

void FakeGCMClient::MessageReceived(const std::string& app_id,
                                    const IncomingMessage& message) {
  if (delegate_)
    delegate_->OnMessageReceived(app_id, message);
}

void FakeGCMClient::MessagesDeleted(const std::string& app_id) {
  if (delegate_)
    delegate_->OnMessagesDeleted(app_id);
}

void FakeGCMClient::MessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  if (delegate_)
    delegate_->OnMessageSendError(app_id, send_error_details);
}

void FakeGCMClient::SendAcknowledgement(const std::string& app_id,
                                        const std::string& message_id) {
  if (delegate_)
    delegate_->OnSendAcknowledged(app_id, message_id);
}

}  // namespace gcm
