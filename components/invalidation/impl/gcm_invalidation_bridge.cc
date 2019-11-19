// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/invalidation/impl/gcm_invalidation_bridge.h"
#include "google_apis/gaia/gaia_constants.h"

namespace invalidation {
namespace {
// For 3rd party developers SenderId should come from application dashboard when
// server side application is registered with Google. Android invalidations use
// legacy format where gmail account can be specificed. Below value is copied
// from Android.
const char kInvalidationsSenderId[] = "ipc.invalidation@gmail.com";
// In Android world AppId is provided by operating system and should
// match package name and hash of application. In desktop world these values
// are arbitrary and not verified/enforced by registration service (yet).
const char kInvalidationsAppId[] = "com.google.chrome.invalidations";

// Cacheinvalidation specific gcm message keys.
const char kContentKey[] = "content";
const char kEchoTokenKey[] = "echo-token";
}  // namespace

// Core should be very simple class that implements GCMNetwrokChannelDelegate
// and passes all calls to GCMInvalidationBridge. All calls should be serialized
// through GCMInvalidationBridge to avoid race conditions.
class GCMInvalidationBridge::Core : public syncer::GCMNetworkChannelDelegate {
 public:
  Core(base::WeakPtr<GCMInvalidationBridge> bridge,
       scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  ~Core() override;

  // syncer::GCMNetworkChannelDelegate implementation.
  void Initialize(ConnectionStateCallback connection_state_callback,
                  base::Closure store_reset_callback) override;
  void RequestToken(RequestTokenCallback callback) override;
  void InvalidateToken(const std::string& token) override;
  void Register(RegisterCallback callback) override;
  void SetMessageReceiver(MessageCallback callback) override;

  void RequestTokenFinished(RequestTokenCallback callback,
                            const GoogleServiceAuthError& error,
                            const std::string& token);

  void RegisterFinished(RegisterCallback callback,
                        const std::string& registration_id,
                        gcm::GCMClient::Result result);

  void OnIncomingMessage(const std::string& message,
                         const std::string& echo_token);

  void OnConnectionStateChanged(bool online);
  void OnStoreReset();

 private:
  base::WeakPtr<GCMInvalidationBridge> bridge_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  MessageCallback message_callback_;
  ConnectionStateCallback connection_state_callback_;
  base::Closure store_reset_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Core> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Core);
};

GCMInvalidationBridge::Core::Core(
    base::WeakPtr<GCMInvalidationBridge> bridge,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : bridge_(bridge), ui_thread_task_runner_(ui_thread_task_runner) {
  // Core is created on UI thread but all calls happen on IO thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

GCMInvalidationBridge::Core::~Core() {}

void GCMInvalidationBridge::Core::Initialize(
    ConnectionStateCallback connection_state_callback,
    base::Closure store_reset_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_state_callback_ = connection_state_callback;
  store_reset_callback_ = store_reset_callback;
  // Pass core WeapPtr and TaskRunner to GCMInvalidationBridge for it to be able
  // to post back.
  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMInvalidationBridge::CoreInitializationDone,
                                bridge_, weak_factory_.GetWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get()));
}

void GCMInvalidationBridge::Core::RequestToken(RequestTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::RequestToken, bridge_, callback));
}

void GCMInvalidationBridge::Core::InvalidateToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::InvalidateToken, bridge_, token));
}

void GCMInvalidationBridge::Core::Register(RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::Register, bridge_, callback));
}

void GCMInvalidationBridge::Core::SetMessageReceiver(MessageCallback callback) {
  message_callback_ = callback;
  ui_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::SubscribeForIncomingMessages,
                     bridge_));
}

void GCMInvalidationBridge::Core::RequestTokenFinished(
    RequestTokenCallback callback,
    const GoogleServiceAuthError& error,
    const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback.Run(error, token);
}

void GCMInvalidationBridge::Core::RegisterFinished(
    RegisterCallback callback,
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback.Run(registration_id, result);
}

void GCMInvalidationBridge::Core::OnIncomingMessage(
    const std::string& message,
    const std::string& echo_token) {
  DCHECK(!message_callback_.is_null());
  message_callback_.Run(message, echo_token);
}

void GCMInvalidationBridge::Core::OnConnectionStateChanged(bool online) {
  if (!connection_state_callback_.is_null()) {
    connection_state_callback_.Run(online);
  }
}

void GCMInvalidationBridge::Core::OnStoreReset() {
  if (!store_reset_callback_.is_null()) {
    store_reset_callback_.Run();
  }
}

GCMInvalidationBridge::GCMInvalidationBridge(
    gcm::GCMDriver* gcm_driver,
    IdentityProvider* identity_provider)
    : gcm_driver_(gcm_driver),
      identity_provider_(identity_provider),
      subscribed_for_incoming_messages_(false) {}

GCMInvalidationBridge::~GCMInvalidationBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (subscribed_for_incoming_messages_) {
    gcm_driver_->RemoveAppHandler(kInvalidationsAppId);
    gcm_driver_->RemoveConnectionObserver(this);
  }
}

std::unique_ptr<syncer::GCMNetworkChannelDelegate>
GCMInvalidationBridge::CreateDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<Core>(weak_factory_.GetWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get());
}

void GCMInvalidationBridge::CoreInitializationDone(
    base::WeakPtr<Core> core,
    scoped_refptr<base::SingleThreadTaskRunner> core_thread_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_ = core;
  core_thread_task_runner_ = core_thread_task_runner;
}

void GCMInvalidationBridge::RequestToken(
    syncer::GCMNetworkChannelDelegate::RequestTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (access_token_fetcher_ != nullptr) {
    // Report previous request as cancelled.
    GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
    std::string access_token;
    core_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GCMInvalidationBridge::Core::RequestTokenFinished,
                       core_, request_token_callback_, error, access_token));
  }
  request_token_callback_ = callback;
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  access_token_fetcher_ = identity_provider_->FetchAccessToken(
      "gcm_network_channel", scopes,
      base::BindOnce(&GCMInvalidationBridge::OnAccessTokenRequestCompleted,
                     base::Unretained(this)));
}

void GCMInvalidationBridge::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    std::string access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::Core::RequestTokenFinished, core_,
                     request_token_callback_, error, access_token));
  request_token_callback_.Reset();
  access_token_fetcher_.reset();
}

void GCMInvalidationBridge::InvalidateToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  identity_provider_->InvalidateAccessToken(scopes, token);
}

void GCMInvalidationBridge::Register(
    syncer::GCMNetworkChannelDelegate::RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No-op if GCMClient is disabled.
  if (gcm_driver_ == nullptr)
    return;

  std::vector<std::string> sender_ids;
  sender_ids.push_back(kInvalidationsSenderId);
  gcm_driver_->Register(kInvalidationsAppId,
                         sender_ids,
                         base::Bind(&GCMInvalidationBridge::RegisterFinished,
                                    weak_factory_.GetWeakPtr(),
                                    callback));
}

void GCMInvalidationBridge::RegisterFinished(
    syncer::GCMNetworkChannelDelegate::RegisterCallback callback,
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMInvalidationBridge::Core::RegisterFinished,
                                core_, callback, registration_id, result));
}

void GCMInvalidationBridge::Unregister() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // No-op if GCMClient is disabled.
  if (gcm_driver_ == nullptr)
    return;

  gcm_driver_->Unregister(kInvalidationsAppId, base::DoNothing());
}

void GCMInvalidationBridge::SubscribeForIncomingMessages() {
  // No-op if GCMClient is disabled.
  if (gcm_driver_ == nullptr)
    return;

  DCHECK(!subscribed_for_incoming_messages_);
  gcm_driver_->AddAppHandler(kInvalidationsAppId, this);
  gcm_driver_->AddConnectionObserver(this);
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::Core::OnConnectionStateChanged,
                     core_, gcm_driver_->IsConnected()));

  subscribed_for_incoming_messages_ = true;
}

void GCMInvalidationBridge::ShutdownHandler() {
  // Nothing to do.
}

void GCMInvalidationBridge::OnStoreReset() {
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::Core::OnStoreReset, core_));
}

void GCMInvalidationBridge::OnMessage(const std::string& app_id,
                                      const gcm::IncomingMessage& message) {
  gcm::MessageData::const_iterator it;
  std::string content;
  std::string echo_token;
  it = message.data.find(kContentKey);
  if (it != message.data.end())
    content = it->second;
  it = message.data.find(kEchoTokenKey);
  if (it != message.data.end())
    echo_token = it->second;

  core_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GCMInvalidationBridge::Core::OnIncomingMessage,
                                core_, content, echo_token));
}

void GCMInvalidationBridge::OnMessagesDeleted(const std::string& app_id) {
  // Cacheinvalidation doesn't use long lived non-collapsable messages with GCM.
  // Android implementation of cacheinvalidation doesn't handle MessagesDeleted
  // callback so this should be no-op in desktop version as well.
}

void GCMInvalidationBridge::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  // cacheinvalidation doesn't send messages over GCM.
  NOTREACHED();
}

void GCMInvalidationBridge::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  // cacheinvalidation doesn't send messages over GCM.
  NOTREACHED();
}

void GCMInvalidationBridge::OnConnected(const net::IPEndPoint& ip_endpoint) {
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::Core::OnConnectionStateChanged,
                     core_, true));
}

void GCMInvalidationBridge::OnDisconnected() {
  core_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GCMInvalidationBridge::Core::OnConnectionStateChanged,
                     core_, false));
}

}  // namespace invalidation
