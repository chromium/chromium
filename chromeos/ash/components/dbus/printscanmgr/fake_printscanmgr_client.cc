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
    const printscanmgr::CupsAddManuallyConfiguredPrinterRequest& request,
    chromeos::DBusMethodCallback<
        printscanmgr::CupsAddManuallyConfiguredPrinterResponse> callback) {
  printers_.insert_or_assign(request.name(), request.ppd_contents());
  printscanmgr::CupsAddManuallyConfiguredPrinterResponse response;
  response.set_result(
      printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_SUCCESS);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakePrintscanmgrClient::CupsAddAutoConfiguredPrinter(
    const printscanmgr::CupsAddAutoConfiguredPrinterRequest& request,
    chromeos::DBusMethodCallback<
        printscanmgr::CupsAddAutoConfiguredPrinterResponse> callback) {
  printers_.insert_or_assign(request.name(), "");
  printscanmgr::CupsAddAutoConfiguredPrinterResponse response;
  response.set_result(
      printscanmgr::AddPrinterResult::ADD_PRINTER_RESULT_SUCCESS);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakePrintscanmgrClient::CupsRemovePrinter(
    const printscanmgr::CupsRemovePrinterRequest& request,
    chromeos::DBusMethodCallback<printscanmgr::CupsRemovePrinterResponse>
        callback,
    base::OnceClosure error_callback) {
  const bool has_printer = base::Contains(printers_, request.name());
  if (has_printer) {
    printers_.erase(request.name());
  }

  printscanmgr::CupsRemovePrinterResponse response;
  response.set_result(has_printer);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakePrintscanmgrClient::CupsRetrievePrinterPpd(
    const printscanmgr::CupsRetrievePpdRequest& request,
    chromeos::DBusMethodCallback<printscanmgr::CupsRetrievePpdResponse>
        callback,
    base::OnceClosure error_callback) {
  auto it = printers_.find(request.name());
  if (it == printers_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(error_callback));
    return;
  }
  printscanmgr::CupsRetrievePpdResponse response;
  response.set_ppd(it->second);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

}  // namespace ash
