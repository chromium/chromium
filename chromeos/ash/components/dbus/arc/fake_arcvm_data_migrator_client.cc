// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arcvm_data_migrator_client.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

// Used to track the fake instance, mirrors the instance in the base class.
FakeArcVmDataMigratorClient* g_instance = nullptr;

}  // namespace

FakeArcVmDataMigratorClient::FakeArcVmDataMigratorClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeArcVmDataMigratorClient::~FakeArcVmDataMigratorClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeArcVmDataMigratorClient* FakeArcVmDataMigratorClient::Get() {
  return g_instance;
}

void FakeArcVmDataMigratorClient::StartMigration(
    const arc::data_migrator::StartMigrationRequest& request,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcVmDataMigratorClient::AddObserver(Observer* observer) {}
void FakeArcVmDataMigratorClient::RemoveObserver(Observer* observer) {}

}  // namespace ash
