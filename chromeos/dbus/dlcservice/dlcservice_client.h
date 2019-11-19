// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_
#define CHROMEOS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dlcservice/dlcservice.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace chromeos {

// DlcserviceClient is used to communicate with the dlcservice daemon which
// manages DLC (Downloadable Content) modules. DlcserviceClient will allow for
// CrOS features to be installed and uninstalled at runtime of the system. If
// more details about dlcservice are required, please consult
// https://chromium.git.corp.google.com/chromiumos/platform2/+/HEAD/dlcservice
class COMPONENT_EXPORT(DLCSERVICE_CLIENT) DlcserviceClient {
 public:
  // The callback used for |Install()|, if the error is something other than
  // |dlcservice::kErrorNone| the call has failed. For large DLC(s) to install,
  // there may be delay between the time of this call and the callback being
  // invoked.
  using InstallCallback = base::OnceCallback<void(
      const std::string& err,
      const dlcservice::DlcModuleList& dlc_module_list)>;

  // The callback used for |Uninstall()|, if the error is something other than
  // |dlcservice::kErrorNone| the call has failed.
  using UninstallCallback = base::OnceCallback<void(const std::string& err)>;

  // The callback used for |GetInstalled()|, if the error is something other
  // than |dlcservice::kErrorNone| the call has failed. It is very rare case for
  // |GetInstalled()| call to fail.
  using GetInstalledCallback = base::OnceCallback<void(
      const std::string& err,
      const dlcservice::DlcModuleList& dlc_module_list)>;

  class Observer : public base::CheckedObserver {
   public:
    // Observers should not dictate the finalization of install based purely on
    // what's notified, but based on the callback passed to |Install()|.
    virtual void OnProgressUpdate(double progress) = 0;
  };

  virtual void AddObserver(Observer* obs) = 0;

  virtual void RemoveObserver(Observer* obs) = 0;

  virtual void NotifyProgressUpdateForTest(double progress) = 0;

  virtual void Install(const dlcservice::DlcModuleList& dlc_module_list,
                       InstallCallback callback) = 0;

  virtual void Uninstall(const std::string& dlc_id,
                         UninstallCallback callback) = 0;

  virtual void GetInstalled(GetInstalledCallback callback) = 0;

  // Creates and initializes the global instance. |bus| must not be nullptr.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be nullptr if not initialized.
  static DlcserviceClient* Get();

 protected:
  friend class DlcserviceClientTest;

  // Initialize/Shutdown should be used instead.
  DlcserviceClient();
  virtual ~DlcserviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(DlcserviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_
