// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/fake_arc_quota_client.h"

#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {
// Used to track the fake instance, mirrors the instance in the base class.
FakeArcQuotaClient* g_instance = nullptr;

}  // namespace

FakeArcQuotaClient::FakeArcQuotaClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeArcQuotaClient::~FakeArcQuotaClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeArcQuotaClient* FakeArcQuotaClient::Get() {
  return g_instance;
}

void FakeArcQuotaClient::GetArcDiskFeatures(
    const ::user_data_auth::GetArcDiskFeaturesRequest& request,
    GetArcDiskFeaturesCallback callback) {
  // Does nothing by default.
}
void FakeArcQuotaClient::GetCurrentSpaceForArcUid(
    const ::user_data_auth::GetCurrentSpaceForArcUidRequest& request,
    GetCurrentSpaceForArcUidCallback callback) {
  // Does nothing by default.
}
void FakeArcQuotaClient::GetCurrentSpaceForArcGid(
    const ::user_data_auth::GetCurrentSpaceForArcGidRequest& request,
    GetCurrentSpaceForArcGidCallback callback) {
  // Does nothing by default.
}
void FakeArcQuotaClient::GetCurrentSpaceForArcProjectId(
    const ::user_data_auth::GetCurrentSpaceForArcProjectIdRequest& request,
    GetCurrentSpaceForArcProjectIdCallback callback) {
  // Does nothing by default.
}

void FakeArcQuotaClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_ || service_reported_not_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeArcQuotaClient::SetServiceIsAvailable(bool is_available) {
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

void FakeArcQuotaClient::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(false);
  }
}

}  // namespace ash
