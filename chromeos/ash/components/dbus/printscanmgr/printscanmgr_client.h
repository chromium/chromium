// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_PRINTSCANMGR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_PRINTSCANMGR_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/bus.h"

namespace ash {

// PrintscanmgrClient is used to communicate with the printscanmgr daemon.
class COMPONENT_EXPORT(PRINTSCANMGR) PrintscanmgrClient
    : public chromeos::DBusClient {
 public:
  // Returns the global instance if initialized. May return null.
  static PrintscanmgrClient* Get();

  // Creates and initializes the global instance. `bus` must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  PrintscanmgrClient(const PrintscanmgrClient&) = delete;
  PrintscanmgrClient& operator=(const PrintscanmgrClient&) = delete;

  ~PrintscanmgrClient() override;

  // A callback to handle the result of CupsAdd[Auto|Manually]ConfiguredPrinter.
  // A negative value denotes a D-Bus library error while non-negative values
  // denote a response from printscanmgr.
  using CupsAddPrinterCallback = base::OnceCallback<void(int32_t)>;

  // Calls CupsAddManuallyConfiguredPrinter. `name` is the printer name. `uri`
  // is the device. `ppd_contents` is the contents of the PPD file used to drive
  // the device. Refer to the comment for `CupsAddPrinterCallback` for details
  // on the format of `callback`.
  virtual void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& ppd_contents,
      CupsAddPrinterCallback callback) = 0;

  // Calls CupsAddAutoConfiguredPrinter. `name` is the printer name. `uri` is
  // the device. Refer to the comment for `CupsAddPrinterCallback` for details
  // on the format of `callback`.
  virtual void CupsAddAutoConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      CupsAddPrinterCallback callback) = 0;

  // A callback to handle the result of CupsRemovePrinter.
  using CupsRemovePrinterCallback = base::OnceCallback<void(bool success)>;

  // Calls CupsRemovePrinter. `name` is the printer name as registered in CUPS.
  // `callback` is called with true if removing the printer from CUPS was
  // successful and false if there was an error. `error_callback` will be called
  // if there was an error communicating with printscanmgr.
  virtual void CupsRemovePrinter(const std::string& name,
                                 CupsRemovePrinterCallback callback,
                                 base::OnceClosure error_callback) = 0;

  // A callback to handle the result of CupsRetrievePrinterPpd.
  using CupsRetrievePrinterPpdCallback =
      base::OnceCallback<void(const std::vector<uint8_t>& ppd)>;

  // Calls the printscanmgr method to retrieve a PPD. `name` is the printer name
  // as registered in CUPS. `callback` is called with a string containing the
  // PPD data. `error_callback` will be called if there was an error retrieving
  // the PPD.
  virtual void CupsRetrievePrinterPpd(const std::string& name,
                                      CupsRetrievePrinterPpdCallback callback,
                                      base::OnceClosure error_callback) = 0;

 protected:
  // Initialize() should be used instead.
  PrintscanmgrClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_PRINTSCANMGR_CLIENT_H_
