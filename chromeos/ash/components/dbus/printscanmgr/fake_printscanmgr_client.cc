// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/printscanmgr/fake_printscanmgr_client.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakePrintscanmgrClient::FakePrintscanmgrClient() = default;
FakePrintscanmgrClient::~FakePrintscanmgrClient() = default;

void FakePrintscanmgrClient::Init(dbus::Bus* bus) {}

void FakePrintscanmgrClient::CupsAddManuallyConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::string& ppd_contents,
    CupsAddPrinterCallback callback) {
  printers_.insert(name);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakePrintscanmgrClient::CupsAddAutoConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    CupsAddPrinterCallback callback) {
  printers_.insert(name);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakePrintscanmgrClient::CupsRemovePrinter(
    const std::string& name,
    CupsRemovePrinterCallback callback,
    base::OnceClosure error_callback) {
  const bool has_printer = base::Contains(printers_, name);
  if (has_printer) {
    printers_.erase(name);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), has_printer));
}

void FakePrintscanmgrClient::CupsRetrievePrinterPpd(
    const std::string& name,
    CupsRetrievePrinterPpdCallback callback,
    base::OnceClosure error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), ppd_data_));
}

void FakePrintscanmgrClient::SetPpdDataForTesting(
    const std::vector<uint8_t>& data) {
  ppd_data_ = data;
}

}  // namespace ash
