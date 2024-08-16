// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"

#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"

namespace ash {

namespace {
// Used to track the fake instance, mirrors the instance in the base class.
FakeCryptohomeMiscClient* g_instance = nullptr;

}  // namespace

FakeCryptohomeMiscClient::FakeCryptohomeMiscClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeCryptohomeMiscClient::~FakeCryptohomeMiscClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeCryptohomeMiscClient* FakeCryptohomeMiscClient::Get() {
  return g_instance;
}

void FakeCryptohomeMiscClient::GetSystemSalt(
    const ::user_data_auth::GetSystemSaltRequest& request,
    GetSystemSaltCallback callback) {
  ::user_data_auth::GetSystemSaltReply reply;
  std::string salt(system_salt_.begin(), system_salt_.end());
  reply.set_salt(salt);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeCryptohomeMiscClient::GetSanitizedUsername(
    const ::user_data_auth::GetSanitizedUsernameRequest& request,
    GetSanitizedUsernameCallback callback) {
  std::optional<::user_data_auth::GetSanitizedUsernameReply> reply;
  reply = BlockingGetSanitizedUsername(request);
  ReturnProtobufMethodCallback(*reply, std::move(callback));
}
void FakeCryptohomeMiscClient::GetLoginStatus(
    const ::user_data_auth::GetLoginStatusRequest& request,
    GetLoginStatusCallback callback) {
  ::user_data_auth::GetLoginStatusReply reply;
  reply.set_owner_user_exists(false);
  reply.set_is_locked_to_single_user(false);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeCryptohomeMiscClient::LockToSingleUserMountUntilReboot(
    const ::user_data_auth::LockToSingleUserMountUntilRebootRequest& request,
    LockToSingleUserMountUntilRebootCallback callback) {
  ::user_data_auth::LockToSingleUserMountUntilRebootReply reply;
  if (cryptohome_error_ == ::user_data_auth::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_NOT_SET);
    is_device_locked_to_single_user_ = true;
  } else {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_FAILED_TO_EXTEND_PCR);
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeCryptohomeMiscClient::GetRsuDeviceId(
    const ::user_data_auth::GetRsuDeviceIdRequest& request,
    GetRsuDeviceIdCallback callback) {
  ::user_data_auth::GetRsuDeviceIdReply reply;
  reply.set_rsu_device_id(rsu_device_id_);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

std::optional<::user_data_auth::GetSanitizedUsernameReply>
FakeCryptohomeMiscClient::BlockingGetSanitizedUsername(
    const ::user_data_auth::GetSanitizedUsernameRequest& request) {
  user_data_auth::GetSanitizedUsernameReply reply;
  if (!service_is_available_) {
    reply.clear_sanitized_username();
  } else {
    cryptohome::AccountIdentifier account;
    account.set_account_id(request.username());
    reply.set_sanitized_username(
        UserDataAuthClient::GetStubSanitizedUsername(account));
  }
  return reply;
}

void FakeCryptohomeMiscClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_ || service_reported_not_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeCryptohomeMiscClient::SetServiceIsAvailable(bool is_available) {
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

void FakeCryptohomeMiscClient::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(false);
  }
}

template <typename ReplyType>
void FakeCryptohomeMiscClient::ReturnProtobufMethodCallback(
    const ReplyType& reply,
    chromeos::DBusMethodCallback<ReplyType> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

// static
std::vector<uint8_t> FakeCryptohomeMiscClient::GetStubSystemSalt() {
  const char kStubSystemSalt[] = "stub_system_salt";
  return std::vector<uint8_t>(kStubSystemSalt,
                              kStubSystemSalt + std::size(kStubSystemSalt) - 1);
}

}  // namespace ash
