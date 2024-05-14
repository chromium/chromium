// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_profile_service.h"

#include <utility>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/gcm_driver/crypto/gcm_encryption_result.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

namespace gcm {

class FakeGCMProfileService::CustomFakeGCMDriver
    : public instance_id::FakeGCMDriverForInstanceID {
 public:
  CustomFakeGCMDriver();

  // Must be called before any other methods.
  void SetService(FakeGCMProfileService* service);

  CustomFakeGCMDriver(const CustomFakeGCMDriver&) = delete;
  CustomFakeGCMDriver& operator=(const CustomFakeGCMDriver&) = delete;

  ~CustomFakeGCMDriver() override;

  void OnRegisterFinished(const std::string& app_id,
                          const std::string& registration_id,
                          GCMClient::Result result);
  void OnSendFinished(const std::string& app_id,
                      const std::string& message_id,
                      GCMClient::Result result);

  // GCMDriver overrides:
  void EncryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& p256dh,
                      const std::string& auth_secret,
                      const std::string& message,
                      EncryptMessageCallback callback) override;

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
                base::TimeDelta time_to_live,
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

  raw_ptr<FakeGCMProfileService> service_ = nullptr;

  // Used to give each registration a unique registration id. Does not decrease
  // when unregister is called.
  int registration_count_ = 0;

  base::WeakPtrFactory<CustomFakeGCMDriver> weak_factory_{
      this};  // Must be last.
};

FakeGCMProfileService::CustomFakeGCMDriver::CustomFakeGCMDriver() = default;

FakeGCMProfileService::CustomFakeGCMDriver::~CustomFakeGCMDriver() = default;

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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CustomFakeGCMDriver::UnregisterFinished,
                                weak_factory_.GetWeakPtr(), app_id, result));
}

void FakeGCMProfileService::CustomFakeGCMDriver::UnregisterWithSenderIdImpl(
    const std::string& app_id,
    const std::string& sender_id) {
  NOTREACHED_IN_MIGRATION() << "This Android-specific method is not yet faked.";
}

void FakeGCMProfileService::CustomFakeGCMDriver::SendImpl(
    const std::string& app_id,
    const std::string& receiver_id,
    const OutgoingMessage& message) {
  if (service_->is_offline_)
    return;  // Drop request.

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CustomFakeGCMDriver::DoSend, weak_factory_.GetWeakPtr(),
                     app_id, receiver_id, message));
}

void FakeGCMProfileService::CustomFakeGCMDriver::EncryptMessage(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& p256dh,
    const std::string& auth_secret,
    const std::string& message,
    EncryptMessageCallback callback) {
  // Pretend that message has been encrypted.
  std::move(callback).Run(GCMEncryptionResult::ENCRYPTED_DRAFT_08, message);
}

void FakeGCMProfileService::CustomFakeGCMDriver::SetService(
    FakeGCMProfileService* service) {
  service_ = service;
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
    base::TimeDelta time_to_live,
    GetTokenCallback callback) {
  if (service_->is_offline_)
    return;  // Drop request.

  instance_id::FakeGCMDriverForInstanceID::GetToken(
      app_id, authorized_entity, scope, time_to_live, std::move(callback));
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

// static
std::unique_ptr<KeyedService> FakeGCMProfileService::Build(
    content::BrowserContext* context) {
  auto custom_driver = std::make_unique<CustomFakeGCMDriver>();
  CustomFakeGCMDriver* custom_driver_ptr = custom_driver.get();

  std::unique_ptr<FakeGCMProfileService> service =
      std::make_unique<FakeGCMProfileService>(std::move(custom_driver));

  custom_driver_ptr->SetService(service.get());
  return service;
}

FakeGCMProfileService::FakeGCMProfileService(
    std::unique_ptr<instance_id::FakeGCMDriverForInstanceID> fake_gcm_driver)
    : GCMProfileService(std::move(fake_gcm_driver)) {}

FakeGCMProfileService::~FakeGCMProfileService() = default;

void FakeGCMProfileService::AddExpectedUnregisterResponse(
    GCMClient::Result result) {
  unregister_responses_.push_back(result);
}

void FakeGCMProfileService::DispatchMessage(const std::string& app_id,
                                            const IncomingMessage& message) {
  GetFakeGCMDriver()->DispatchMessage(app_id, message);
}

instance_id::FakeGCMDriverForInstanceID*
FakeGCMProfileService::GetFakeGCMDriver() {
  return static_cast<instance_id::FakeGCMDriverForInstanceID*>(driver());
}

}  // namespace gcm
