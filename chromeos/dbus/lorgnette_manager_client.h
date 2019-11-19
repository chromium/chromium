// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_LORGNETTE_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_LORGNETTE_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace chromeos {

// LorgnetteManagerClient is used to communicate with the lorgnette
// document scanning daemon.
class COMPONENT_EXPORT(CHROMEOS_DBUS) LorgnetteManagerClient
    : public DBusClient {
 public:
  // The property information for each scanner retured by ListScanners.
  using ScannerTableEntry = std::map<std::string, std::string>;
  using ScannerTable = std::map<std::string, ScannerTableEntry>;

  // Attributes provided to a scan request.
  struct ScanProperties {
    std::string mode;  // Can be "Color", "Gray", or "Lineart".
    int resolution_dpi = 0;
  };

  ~LorgnetteManagerClient() override;

  // Gets a list of scanners from the lorgnette manager.
  virtual void ListScanners(DBusMethodCallback<ScannerTable> callback) = 0;

  // Request a scanned image and calls |callback| when completed with a string
  // pointing at the scanned image data.  Image data will be stored in the .png
  // format.
  virtual void ScanImageToString(std::string device_name,
                                 const ScanProperties& properties,
                                 DBusMethodCallback<std::string> callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<LorgnetteManagerClient> Create();

 protected:
  // Create() should be used instead.
  LorgnetteManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(LorgnetteManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_LORGNETTE_MANAGER_CLIENT_H_
