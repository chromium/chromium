// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace chromeos {
namespace {

// Posts `callback` on the current thread's task runner, passing it the
// `reply` message.
template <class ReplyType>
void PostProtoResponse(base::OnceCallback<void(const ReplyType&)> callback,
                       const ReplyType& reply) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

}  // namespace

FakeTpmManagerClient::FakeTpmManagerClient() {
  // By design, TPM is configured as "enabled" and "owned" for backward
  // compatibility of what fake cryptohome client used to do.
  nonsensitive_status_reply_.set_is_enabled(true);
  nonsensitive_status_reply_.set_is_owned(true);
}

FakeTpmManagerClient::~FakeTpmManagerClient() = default;

void FakeTpmManagerClient::GetTpmNonsensitiveStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusRequest& request,
    GetTpmNonsensitiveStatusCallback callback) {
  ::tpm_manager::GetTpmNonsensitiveStatusReply reply;
  if (nonsensitive_status_dbus_error_count_ != 0) {
    --nonsensitive_status_dbus_error_count_;
    reply.set_status(::tpm_manager::STATUS_DBUS_ERROR);
  } else {
    reply = nonsensitive_status_reply_;
  }
  PostProtoResponse(std::move(callback), reply);
}

void FakeTpmManagerClient::GetVersionInfo(
    const ::tpm_manager::GetVersionInfoRequest& request,
    GetVersionInfoCallback callback) {
  PostProtoResponse(std::move(callback), version_info_reply_);
}

void FakeTpmManagerClient::GetSupportedFeatures(
    const ::tpm_manager::GetSupportedFeaturesRequest& request,
    GetSupportedFeaturesCallback callback) {
  PostProtoResponse(std::move(callback), supported_features_reply_);
}

void FakeTpmManagerClient::GetDictionaryAttackInfo(
    const ::tpm_manager::GetDictionaryAttackInfoRequest& request,
    GetDictionaryAttackInfoCallback callback) {
  PostProtoResponse(std::move(callback), dictionary_attack_info_reply_);
}

void FakeTpmManagerClient::TakeOwnership(
    const ::tpm_manager::TakeOwnershipRequest& request,
    TakeOwnershipCallback callback) {
  ++take_ownership_count_;
  PostProtoResponse(std::move(callback), ::tpm_manager::TakeOwnershipReply());
}

void FakeTpmManagerClient::ClearStoredOwnerPassword(
    const ::tpm_manager::ClearStoredOwnerPasswordRequest& request,
    ClearStoredOwnerPasswordCallback callback) {
  ++clear_stored_owner_password_count_;
  PostProtoResponse(std::move(callback),
                    ::tpm_manager::ClearStoredOwnerPasswordReply());
}

void FakeTpmManagerClient::ClearTpm(
    const ::tpm_manager::ClearTpmRequest& request,
    ClearTpmCallback callback) {
  ++clear_tpm_count_;
  PostProtoResponse(std::move(callback), ::tpm_manager::ClearTpmReply());
}

void FakeTpmManagerClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeTpmManagerClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

TpmManagerClient::TestInterface* FakeTpmManagerClient::GetTestInterface() {
  return this;
}

::tpm_manager::GetTpmNonsensitiveStatusReply*
FakeTpmManagerClient::mutable_nonsensitive_status_reply() {
  return &nonsensitive_status_reply_;
}

void FakeTpmManagerClient::set_non_nonsensitive_status_dbus_error_count(
    int count) {
  nonsensitive_status_dbus_error_count_ = count;
}

::tpm_manager::GetVersionInfoReply*
FakeTpmManagerClient::mutable_version_info_reply() {
  return &version_info_reply_;
}

::tpm_manager::GetSupportedFeaturesReply*
FakeTpmManagerClient::mutable_supported_features_reply() {
  return &supported_features_reply_;
}
::tpm_manager::GetDictionaryAttackInfoReply*
FakeTpmManagerClient::mutable_dictionary_attack_info_reply() {
  return &dictionary_attack_info_reply_;
}

int FakeTpmManagerClient::take_ownership_count() const {
  return take_ownership_count_;
}

int FakeTpmManagerClient::clear_stored_owner_password_count() const {
  return clear_stored_owner_password_count_;
}

int FakeTpmManagerClient::clear_tpm_count() const {
  return clear_tpm_count_;
}

void FakeTpmManagerClient::EmitOwnershipTakenSignal() {
  for (auto& observer : observer_list_) {
    observer.OnOwnershipTaken();
  }
}

}  // namespace chromeos
