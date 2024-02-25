// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/human_presence/fake_human_presence_dbus_client.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

FakeHumanPresenceDBusClient* g_fake_instance = nullptr;

}  // namespace

// static
FakeHumanPresenceDBusClient* FakeHumanPresenceDBusClient::Get() {
  return g_fake_instance;
}

FakeHumanPresenceDBusClient::FakeHumanPresenceDBusClient() {
  DCHECK(!g_fake_instance);
  g_fake_instance = this;
}

FakeHumanPresenceDBusClient::~FakeHumanPresenceDBusClient() {
  DCHECK_EQ(this, g_fake_instance);
  g_fake_instance = nullptr;
}

void FakeHumanPresenceDBusClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeHumanPresenceDBusClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeHumanPresenceDBusClient::GetResultHpsSense(GetResultCallback cb) {
  ++hps_sense_count_;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), hps_sense_result_));
}

void FakeHumanPresenceDBusClient::GetResultHpsNotify(GetResultCallback cb) {
  ++hps_notify_count_;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), hps_notify_result_));
}

void FakeHumanPresenceDBusClient::EnableHpsSense(
    const hps::FeatureConfig& config) {
  ++enable_hps_sense_count_;
}

void FakeHumanPresenceDBusClient::DisableHpsSense() {
  ++disable_hps_sense_count_;
}

void FakeHumanPresenceDBusClient::EnableHpsNotify(
    const hps::FeatureConfig& config) {
  ++enable_hps_notify_count_;
}

void FakeHumanPresenceDBusClient::DisableHpsNotify() {
  ++disable_hps_notify_count_;
}

void FakeHumanPresenceDBusClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback cb) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), hps_service_is_available_));
}

void FakeHumanPresenceDBusClient::Restart() {
  for (auto& observer : observers_) {
    observer.OnRestart();
  }
}

void FakeHumanPresenceDBusClient::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnShutdown();
  }
}

void FakeHumanPresenceDBusClient::Reset() {
  hps_notify_result_.reset();
  hps_notify_count_ = 0;
  enable_hps_notify_count_ = 0;
  disable_hps_notify_count_ = 0;
  enable_hps_sense_count_ = 0;
  disable_hps_sense_count_ = 0;
  hps_service_is_available_ = false;

  observers_.Clear();
}

}  // namespace ash
