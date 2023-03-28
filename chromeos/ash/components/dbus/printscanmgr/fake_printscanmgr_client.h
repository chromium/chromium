// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_FAKE_PRINTSCANMGR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_FAKE_PRINTSCANMGR_CLIENT_H_

#include <stdint.h>

#include <set>
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
      const std::string& name,
      const std::string& uri,
      const std::string& ppd_contents,
      CupsAddPrinterCallback callback) override;
  void CupsAddAutoConfiguredPrinter(const std::string& name,
                                    const std::string& uri,
                                    CupsAddPrinterCallback callback) override;
  void CupsRemovePrinter(const std::string& name,
                         CupsRemovePrinterCallback callback,
                         base::OnceClosure error_callback) override;
  void CupsRetrievePrinterPpd(const std::string& name,
                              CupsRetrievePrinterPpdCallback callback,
                              base::OnceClosure error_callback) override;

  // Sets PPD data that will be returned by CupsRetrievePrinterPpd for testing.
  void SetPpdDataForTesting(const std::vector<uint8_t>& data);

 private:
  std::set<std::string> printers_;
  std::vector<uint8_t> ppd_data_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_FAKE_PRINTSCANMGR_CLIENT_H_
