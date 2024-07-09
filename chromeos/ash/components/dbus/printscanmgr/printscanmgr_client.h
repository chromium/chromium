// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_PRINTSCANMGR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_PRINTSCANMGR_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_service.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
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

  // Creates and overrides any existing instance with a fake global instance for
  // tests.
  static void InitializeFakeForTest();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  PrintscanmgrClient(const PrintscanmgrClient&) = delete;
  PrintscanmgrClient& operator=(const PrintscanmgrClient&) = delete;

  ~PrintscanmgrClient() override;

  // Requests that cupsd adds a printer which needs to be manually configured.
  virtual void CupsAddManuallyConfiguredPrinter(
      const printscanmgr::CupsAddManuallyConfiguredPrinterRequest& request,
      chromeos::DBusMethodCallback<
          printscanmgr::CupsAddManuallyConfiguredPrinterResponse> callback) = 0;

  // Requests that cupsd adds a printer which can be autoconfigured.
  virtual void CupsAddAutoConfiguredPrinter(
      const printscanmgr::CupsAddAutoConfiguredPrinterRequest& request,
      chromeos::DBusMethodCallback<
          printscanmgr::CupsAddAutoConfiguredPrinterResponse> callback) = 0;

  // Requests that cupsd removes a printer. `error_callback` will be called if
  // there was an error communicating with printscanmgr or encoding the request.
  virtual void CupsRemovePrinter(
      const printscanmgr::CupsRemovePrinterRequest& request,
      chromeos::DBusMethodCallback<printscanmgr::CupsRemovePrinterResponse>
          callback,
      base::OnceClosure error_callback) = 0;

  // Retrieves the requested PPD. `error_callback` will be called if there was
  // an error retrieving the PPD or encoding the request.
  virtual void CupsRetrievePrinterPpd(
      const printscanmgr::CupsRetrievePpdRequest& request,
      chromeos::DBusMethodCallback<printscanmgr::CupsRetrievePpdResponse>
          callback,
      base::OnceClosure error_callback) = 0;

 protected:
  // Initialize() should be used instead.
  PrintscanmgrClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PRINTSCANMGR_PRINTSCANMGR_CLIENT_H_
