// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {
namespace {

// Posts `callback` on the current thread's task runner, passing it the
// `reply` message.
template <class ReplyType>
void PostProtoResponse(base::OnceCallback<void(const ReplyType&)> callback,
                       const ReplyType& reply) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

}  // namespace

FakeTpmManagerClient::FakeTpmManagerClient() = default;

FakeTpmManagerClient::~FakeTpmManagerClient() = default;

void FakeTpmManagerClient::GetTpmNonsensitiveStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusRequest& request,
    GetTpmNonsensitiveStatusCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTpmManagerClient::GetVersionInfo(
    const ::tpm_manager::GetVersionInfoRequest& request,
    GetVersionInfoCallback callback) {
  PostProtoResponse(std::move(callback), version_info_reply_);
}

void FakeTpmManagerClient::GetDictionaryAttackInfo(
    const ::tpm_manager::GetDictionaryAttackInfoRequest& request,
    GetDictionaryAttackInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTpmManagerClient::TakeOwnership(
    const ::tpm_manager::TakeOwnershipRequest& request,
    TakeOwnershipCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTpmManagerClient::ClearStoredOwnerPassword(
    const ::tpm_manager::ClearStoredOwnerPasswordRequest& request,
    ClearStoredOwnerPasswordCallback callback) {
  NOTIMPLEMENTED();
}

TpmManagerClient::TestInterface* FakeTpmManagerClient::GetTestInterface() {
  return this;
}

::tpm_manager::GetVersionInfoReply*
FakeTpmManagerClient::mutable_version_info_reply() {
  return &version_info_reply_;
}

}  // namespace chromeos
