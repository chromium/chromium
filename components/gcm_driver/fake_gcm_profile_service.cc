// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_profile_service.h"

#include <utility>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

namespace gcm {

class FakeGCMProfileService::CustomFakeGCMDriver
    : public instance_id::FakeGCMDriverForInstanceID {
 public:
  explicit CustomFakeGCMDriver(FakeGCMProfileService* service);
  ~CustomFakeGCMDriver() override;

  void OnRegisterFinished(const std::string& app_id,
                          const std::string& registration_id,
                          GCMClient::Result result);
  void OnSendFinished(const std::string& app_id,
                      const std::string& message_id,
                      GCMClient::Result result);

  void OnDispatchMessage(const std::string& app_id,
                         const IncomingMessage& message);

  // instance_id::FakeGCMDriverForInstanceID overrides:
  void SendWebPushMessage(const std::string& app_id,
                          const std::string& authorized_entity,
                          const std::string& p256dh,
                          const std::string& auth_secret,
                          const std::string& fcm_token,
                          crypto::ECPrivateKey* vapid_key,
                          WebPushMessage message,
                          WebPushCallback callback) override;

 protected:
  // FakeGCMDriver overrides:
  void RegisterImpl(const std::string& app_id,
                    const std::vector<std::string>& sender_ids) override;
  void UnregisterImpl(const std::string& app_id) override;
  void UnregisterWithSenderIdImpl(const std::string& app_id,
                                  const std::string& sender_id) override;
  void SendImpl(const std::string& app_id,
                const std::string& receiver_id,
                const OutgoingMessage& message) override;

  // FakeGCMDriverForInstanceID overrides:
  void GetToken(const std::string& app_id,
                const std::string& authorized_entity,
                const std::string& scope,
                const std::map<std::string, std::string>& options,
                GetTokenCallback callback) override;
  void DeleteToken(const std::string& app_id,
                   const std::string& authorized_entity,
                   const std::string& scope,
                   DeleteTokenCallback callback) override;

 private:
  void DoRegister(const std::string& app_id,
                  const std::vector<std::string>& sender_ids,
                  const std::string& registration_id);
  void DoSend(const std::string& app_id,
              const std::string& receiver_id,
              const OutgoingMessage& message);

  FakeGCMProfileService* service_;

  // Used to give each registration a unique registration id. Does not decrease
  // when unregister is called.
  int registration_count_ = 0;

  base::WeakPtrFactory<CustomFakeGCMDriver> weak_factory_{
      this};  // Must be last.

  DISALLOW_COPY_AND_ASSIGN(CustomFakeGCMDriver);
};

FakeGCMProfileService::CustomFakeGCMDriver::CustomFakeGCMDriver(
    FakeGCMProfileService* service)
    : instance_id::FakeGCMDriverForInstanceID(
          base::ThreadTaskRunnerHandle::Get()),
      service_(service) {}

FakeGCMProfileService::CustomFakeGCMDriver::~CustomFakeGCMDriver() {}

void FakeGCMProfileService::CustomFakeGCMDriver::RegisterImpl(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  if (service_->is_offline_)
    return;  // Drop request.

  // Generate fake registration IDs, encoding the number of sender IDs (used by
  // GcmApiTest.RegisterValidation), then an incrementing count (even for the
  // same app_id - there's no caching) so tests can distinguish registrations.
  std::string registration_id = base::StringPrintf(
      "%" PRIuS "-%d", sender_ids.size(), registration_count_);
  ++registration_count_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CustomFakeGCMDriver::DoRegister,
                                weak_factory_.GetWeakPtr(), app_id, sender_ids,
                                registration_id));
}

void FakeGCMProfileService::CustomFakeGCMDriver::DoRegister(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& registration_id) {
  if (service_->collect_) {
    service_->last_registered_app_id_ = app_id;
    service_->last_registered_sender_ids_ = sender_ids;
  }
  RegisterFinished(app_id, registration_id, GCMClient::SUCCESS);
}

void FakeGCMProfileService::CustomFakeGCMDriver::UnregisterImpl(
    const std::string& app_id) {
  if (service_->is_offline_)
    return;  // Drop request.

  GCMClient::Result result = GCMClient::SUCCESS;
  if (!service_->unregister_responses_.empty()) {
    result = service_->unregister_responses_.front();
    service_->unregister_responses_.pop_front();
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CustomFakeGCMDriver::UnregisterFinished,
                                weak_factory_.GetWeakPtr(), app_id, result));
}

void FakeGCMProfileService::CustomFakeGCMDriver::UnregisterWithSenderIdImpl(
    const std::string& app_id,
    const std::string& sender_id) {
  NOTREACHED() << "This Android-specific method is not yet faked.";
}

void FakeGCMProfileService::CustomFakeGCMDriver::SendImpl(
    const std::string& app_id,
    const std::string& receiver_id,
    const OutgoingMessage& message) {
  if (service_->is_offline_)
    return;  // Drop request.

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CustomFakeGCMDriver::DoSend, weak_factory_.GetWeakPtr(),
                     app_id, receiver_id, message));
}

void FakeGCMProfileService::CustomFakeGCMDriver::SendWebPushMessage(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& p256dh,
    const std::string& auth_secret,
    const std::string& fcm_token,
    crypto::ECPrivateKey* vapid_key,
    WebPushMessage message,
    WebPushCallback callback) {
  if (service_->collect_) {
    service_->last_receiver_id_ = fcm_token;
    service_->last_web_push_message_ = std::move(message);
  }
}

void FakeGCMProfileService::CustomFakeGCMDriver::DoSend(
    const std::string& app_id,
    const std::string& receiver_id,
    const OutgoingMessage& message) {
  if (service_->collect_) {
    service_->last_sent_message_ = message;
    service_->last_receiver_id_ = receiver_id;
  }
  SendFinished(app_id, message.id, GCMClient::SUCCESS);
}

void FakeGCMProfileService::CustomFakeGCMDriver::GetToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    const std::map<std::string, std::string>& options,
    GetTokenCallback callback) {
  if (service_->is_offline_)
    return;  // Drop request.

  instance_id::FakeGCMDriverForInstanceID::GetToken(
      app_id, authorized_entity, scope, options, std::move(callback));
}

void FakeGCMProfileService::CustomFakeGCMDriver::DeleteToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    DeleteTokenCallback callback) {
  if (service_->is_offline_)
    return;  // Drop request.

  instance_id::FakeGCMDriverForInstanceID::DeleteToken(
      app_id, authorized_entity, scope, std::move(callback));
}

void FakeGCMProfileService::CustomFakeGCMDriver::OnDispatchMessage(
    const std::string& app_id,
    const IncomingMessage& message) {
  DispatchMessage(app_id, message);
}

// static
std::unique_ptr<KeyedService> FakeGCMProfileService::Build(
    content::BrowserContext* context) {
  std::unique_ptr<FakeGCMProfileService> service =
      std::make_unique<FakeGCMProfileService>();
  service->SetDriverForTesting(
      std::make_unique<CustomFakeGCMDriver>(service.get()));

  return service;
}

FakeGCMProfileService::FakeGCMProfileService() = default;

FakeGCMProfileService::~FakeGCMProfileService() = default;

void FakeGCMProfileService::AddExpectedUnregisterResponse(
    GCMClient::Result result) {
  unregister_responses_.push_back(result);
}

void FakeGCMProfileService::DispatchMessage(const std::string& app_id,
                                            const IncomingMessage& message) {
  CustomFakeGCMDriver* custom_driver =
      static_cast<CustomFakeGCMDriver*>(driver());
  custom_driver->OnDispatchMessage(app_id, message);
}

}  // namespace gcm
