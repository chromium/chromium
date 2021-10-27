// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hps/fake_hps_dbus_client.h"

#include "base/bind.h"
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

void FakeHpsDBusClient::AddObserver(Observer* observer) {}

void FakeHpsDBusClient::RemoveObserver(Observer* observer) {}

void FakeHpsDBusClient::GetResultHpsNotify(GetResultHpsNotifyCallback cb) {
  ++hps_notify_count_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), hps_notify_result_));
}

}  // namespace chromeos
