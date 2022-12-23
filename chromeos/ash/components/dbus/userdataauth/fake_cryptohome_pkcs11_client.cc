// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_pkcs11_client.h"

#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {
// Used to track the fake instance, mirrors the instance in the base class.
FakeCryptohomePkcs11Client* g_instance = nullptr;

}  // namespace

FakeCryptohomePkcs11Client::FakeCryptohomePkcs11Client() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeCryptohomePkcs11Client::~FakeCryptohomePkcs11Client() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeCryptohomePkcs11Client* FakeCryptohomePkcs11Client::Get() {
  return g_instance;
}

void FakeCryptohomePkcs11Client::Pkcs11IsTpmTokenReady(
    const ::user_data_auth::Pkcs11IsTpmTokenReadyRequest& request,
    Pkcs11IsTpmTokenReadyCallback callback) {
  ::user_data_auth::Pkcs11IsTpmTokenReadyReply reply;
  reply.set_ready(true);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}
void FakeCryptohomePkcs11Client::Pkcs11GetTpmTokenInfo(
    const ::user_data_auth::Pkcs11GetTpmTokenInfoRequest& request,
    Pkcs11GetTpmTokenInfoCallback callback) {
  const char kStubTPMTokenName[] = "StubTPMTokenName";
  const char kStubUserPin[] = "012345";
  const int kStubSlot = 0;

  ::user_data_auth::Pkcs11GetTpmTokenInfoReply reply;
  reply.mutable_token_info()->set_slot(kStubSlot);
  reply.mutable_token_info()->set_user_pin(kStubUserPin);
  reply.mutable_token_info()->set_label(kStubTPMTokenName);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

void FakeCryptohomePkcs11Client::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_ || service_reported_not_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeCryptohomePkcs11Client::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available) {
    return;
  }

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(true);
  }
}

void FakeCryptohomePkcs11Client::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(false);
  }
}

}  // namespace ash
