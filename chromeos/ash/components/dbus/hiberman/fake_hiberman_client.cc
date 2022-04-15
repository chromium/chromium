// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hiberman/fake_hiberman_client.h"

#include <utility>

#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

namespace {

// Used to track the fake instance, mirrors the instance in the base class.
FakeHibermanClient* g_instance = nullptr;

}  // namespace

FakeHibermanClient::FakeHibermanClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeHibermanClient::~FakeHibermanClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeHibermanClient* FakeHibermanClient::Get() {
  return g_instance;
}

void FakeHibermanClient::ResumeFromHibernate(
    const std::string& account_id,
    ResumeFromHibernateCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeHibermanClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  // Don't call us, we'll call you ;)
}

}  // namespace ash
