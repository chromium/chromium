// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_FAKE_PRINTSCANMGR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_FAKE_PRINTSCANMGR_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"

namespace ash {

// Implementation of PrintscanmgrClient used for testing.
class COMPONENT_EXPORT(PRINTSCANMGR) FakePrintscanmgrClient
    : public PrintscanmgrClient {
 public:
  FakePrintscanmgrClient();
  FakePrintscanmgrClient(const FakePrintscanmgrClient&) = delete;
  FakePrintscanmgrClient& operator=(const FakePrintscanmgrClient&) = delete;
  ~FakePrintscanmgrClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus) override;

  // PrintscanmgrClient overrides:
  void CupsAddManuallyConfiguredPrinter(
      const printscanmgr::CupsAddManuallyConfiguredPrinterRequest& request,
      chromeos::DBusMethodCallback<
          printscanmgr::CupsAddManuallyConfiguredPrinterResponse> callback)
      override;
  void CupsAddAutoConfiguredPrinter(
      const printscanmgr::CupsAddAutoConfiguredPrinterRequest& request,
      chromeos::DBusMethodCallback<
          printscanmgr::CupsAddAutoConfiguredPrinterResponse> callback)
      override;
  void CupsRemovePrinter(
      const printscanmgr::CupsRemovePrinterRequest& request,
      chromeos::DBusMethodCallback<printscanmgr::CupsRemovePrinterResponse>
          callback,
      base::OnceClosure error_callback) override;
  // Returns PPD set in CupsAddManuallyConfiguredPrinter or an empty string if
  // the printer was added with CupsAddAutoConfiguredPrinter. If the printer
  // does not exists then `error_callback` is called.
  void CupsRetrievePrinterPpd(
      const printscanmgr::CupsRetrievePpdRequest& request,
      chromeos::DBusMethodCallback<printscanmgr::CupsRetrievePpdResponse>
          callback,
      base::OnceClosure error_callback) override;

 private:
  // Stores printer's name as a key and PPD content as a value.
  std::map<std::string, std::string> printers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_FAKE_PRINTSCANMGR_CLIENT_H_
