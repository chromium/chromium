// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hps/fake_hps_dbus_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

FakeHpsDBusClient* g_fake_instance = nullptr;

}  // namespace

// static
FakeHpsDBusClient* FakeHpsDBusClient::Get() {
  return g_fake_instance;
}

FakeHpsDBusClient::FakeHpsDBusClient() {
  DCHECK(!g_fake_instance);
  g_fake_instance = this;
}

FakeHpsDBusClient::~FakeHpsDBusClient() {
  DCHECK_EQ(this, g_fake_instance);
  g_fake_instance = nullptr;
}

void FakeHpsDBusClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeHpsDBusClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeHpsDBusClient::GetResultHpsSense(GetResultCallback cb) {
  ++hps_sense_count_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), hps_sense_result_));
}

void FakeHpsDBusClient::GetResultHpsNotify(GetResultCallback cb) {
  ++hps_notify_count_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), hps_notify_result_));
}

void FakeHpsDBusClient::EnableHpsSense(const hps::FeatureConfig& config) {
  ++enable_hps_sense_count_;
}

void FakeHpsDBusClient::DisableHpsSense() {
  ++disable_hps_sense_count_;
}

void FakeHpsDBusClient::EnableHpsNotify(const hps::FeatureConfig& config) {
  ++enable_hps_notify_count_;
}

void FakeHpsDBusClient::DisableHpsNotify() {
  ++disable_hps_notify_count_;
}

void FakeHpsDBusClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback cb) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), hps_service_is_available_));
}

void FakeHpsDBusClient::Restart() {
  for (auto& observer : observers_) {
    observer.OnRestart();
  }
}

void FakeHpsDBusClient::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnShutdown();
  }
}

void FakeHpsDBusClient::Reset() {
  hps_notify_result_.reset();
  hps_notify_count_ = 0;
  enable_hps_notify_count_ = 0;
  disable_hps_notify_count_ = 0;
  enable_hps_sense_count_ = 0;
  disable_hps_sense_count_ = 0;
  hps_service_is_available_ = false;

  observers_.Clear();
}

}  // namespace chromeos
