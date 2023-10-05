// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hiberman/fake_hiberman_client.h"

#include <utility>

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

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

bool FakeHibermanClient::IsAlive() const {
  return true;
}

bool FakeHibermanClient::IsEnabled() const {
  return true;
}

bool FakeHibermanClient::IsHibernateToS4Enabled() const {
  return true;
}

void FakeHibermanClient::ResumeFromHibernate(
    const std::string& account_id,
    const std::string& auth_session_id) {}

void FakeHibermanClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  // Sure, the service is available now!
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeHibermanClient::AbortResumeHibernate(const std::string& reason) {}

}  // namespace ash
