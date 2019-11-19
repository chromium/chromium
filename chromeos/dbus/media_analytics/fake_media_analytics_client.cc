// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/media_analytics/fake_media_analytics_client.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

namespace {

FakeMediaAnalyticsClient* g_instance = nullptr;

}  // namespace

FakeMediaAnalyticsClient::FakeMediaAnalyticsClient() : process_running_(false) {
  current_state_.set_status(mri::State::UNINITIALIZED);
  DCHECK(!g_instance);
  g_instance = this;
}

FakeMediaAnalyticsClient::~FakeMediaAnalyticsClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeMediaAnalyticsClient* FakeMediaAnalyticsClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

bool FakeMediaAnalyticsClient::FireMediaPerceptionEvent(
    const mri::MediaPerception& media_perception) {
  if (!process_running_)
    return false;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMediaAnalyticsClient::OnMediaPerception,
                     weak_ptr_factory_.GetWeakPtr(), media_perception));
  return true;
}

void FakeMediaAnalyticsClient::SetDiagnostics(
    const mri::Diagnostics& diagnostics) {
  diagnostics_ = diagnostics;
}

void FakeMediaAnalyticsClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeMediaAnalyticsClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeMediaAnalyticsClient::GetState(
    DBusMethodCallback<mri::State> callback) {
  if (!process_running_) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMediaAnalyticsClient::OnState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeMediaAnalyticsClient::SetState(
    const mri::State& state,
    DBusMethodCallback<mri::State> callback) {
  if (!process_running_) {
    std::move(callback).Run(base::nullopt);
    return;
  }
  DCHECK(state.has_status()) << "Trying to set state without status.";
  DCHECK(state.status() == mri::State::SUSPENDED ||
         state.status() == mri::State::RUNNING ||
         state.status() == mri::State::RESTARTING)
      << "Trying set state to something other than RUNNING, SUSPENDED or "
         "RESTARTING.";
  current_state_ = state;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMediaAnalyticsClient::OnState,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeMediaAnalyticsClient::SetStateSuspended() {
  if (!process_running_) {
    return;
  }
  mri::State suspended;
  suspended.set_status(mri::State::SUSPENDED);
  current_state_ = suspended;
}

void FakeMediaAnalyticsClient::OnState(
    DBusMethodCallback<mri::State> callback) {
  std::move(callback).Run(current_state_);
}

void FakeMediaAnalyticsClient::GetDiagnostics(
    DBusMethodCallback<mri::Diagnostics> callback) {
  if (!process_running_) {
    LOG(ERROR) << "Fake media analytics process not running.";
    std::move(callback).Run(base::nullopt);
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMediaAnalyticsClient::OnGetDiagnostics,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeMediaAnalyticsClient::BootstrapMojoConnection(
    base::ScopedFD file_descriptor,
    VoidDBusMethodCallback callback) {
  // Fake that the mojo connection has been successfully established.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeMediaAnalyticsClient::OnGetDiagnostics(
    DBusMethodCallback<mri::Diagnostics> callback) {
  std::move(callback).Run(diagnostics_);
}

void FakeMediaAnalyticsClient::OnMediaPerception(
    const mri::MediaPerception& media_perception) {
  for (auto& observer : observer_list_)
    observer.OnDetectionSignal(media_perception);
}

}  // namespace chromeos
