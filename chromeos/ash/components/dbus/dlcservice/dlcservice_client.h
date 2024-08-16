// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash {

// This class is a singleton and should be accessed using `Get()`.
// DlcserviceClient is used to communicate with the dlcservice daemon which
// manages DLC (Downloadable Content) modules. DlcserviceClient will allow for
// CrOS features to be installed and uninstalled at runtime of the system. If
// more details about dlcservice are required, please consult
// https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/dlcservice
class COMPONENT_EXPORT(DLCSERVICE_CLIENT) DlcserviceClient {
 public:
  // Observer class for objects that need to know the change in the state of
  // DLCs like UI, etc.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Is called whenever the state of a DLC is changed. Changing the
    // installation progress of the DLC constitues as a state change.
    virtual void OnDlcStateChanged(const dlcservice::DlcState& dlc_state) = 0;

   protected:
    Observer() = default;
  };

  // This object is returned as the result of DLC install success or failure.
  struct InstallResult {
    // The error associated with the install. `dlcservice::kErrorNone` indicates
    // a success. Any other error code, indicates a failure.
    std::string error;
    // The unique DLC ID which was requested to be installed.
    std::string dlc_id;
    // The path where the DLC is available for users to use.
    std::string root_path;
  };

  // The callback used for `Install()`. For large DLC(s) to install, there may
  // be a delay between the time of this call and the callback being invoked.
  using InstallCallback =
      base::OnceCallback<void(const InstallResult& install_result)>;

  // The callback used for `Install()`, if the caller wants to listen in on the
  // progress of their download/install. If the caller only cares for whether
  // the install is complete or not, the caller can pass in `RepeatingCallback`
  // that is a no-op.
  using ProgressCallback = base::RepeatingCallback<void(double progress)>;

  // The callback used for `Uninstall()`, if the error is something other than
  // `dlcservice::kErrorNone` the call has failed.
  using UninstallCallback = base::OnceCallback<void(std::string_view err)>;

  // The callback used for `Purge()`, if the error is something other than
  // `dlcservice::kErrorNone` the call has failed.
  using PurgeCallback = base::OnceCallback<void(std::string_view err)>;

  // The callback used for `GetDlcState()`, if the error is something other
  // than `dlcservice::kErrorNone` the call has failed.
  using GetDlcStateCallback =
      base::OnceCallback<void(std::string_view err,
                              const dlcservice::DlcState& dlc_state)>;

  // The callback used for `GetExistingDlcs()`, if the error is something other
  // than `dlcservice::kErrorNone` the call has failed. It is a very rare case
  // for `GetExistingDlcs()` call to fail.
  using GetExistingDlcsCallback = base::OnceCallback<void(
      std::string_view err,
      const dlcservice::DlcsWithContent& dlcs_with_content)>;

  // Installs the DLC passed in while reporting progress through the progress
  // callback and only calls install callback on install success/failure.
  virtual void Install(const dlcservice::InstallRequest& install_request,
                       InstallCallback callback,
                       ProgressCallback progress_callback) = 0;

  // Uninstalls a single DLC and calls the callback with indication of
  // success/failure. Uninstall is the same as `Purge()`.
  virtual void Uninstall(const std::string& dlc_id,
                         UninstallCallback callback) = 0;

  // Purges a single DLC and calls the callback with indication of
  // success/failure. Purging removes the DLC entirely from disk, regardless if
  // the DLC has been uninstalled already.
  virtual void Purge(const std::string& dlc_id,
                     PurgeCallback purge_callback) = 0;

  // Returns the state of a single DLC. Including information
  // such as installation state, id, and verification state.
  virtual void GetDlcState(const std::string& dlc_id,
                           GetDlcStateCallback callback) = 0;

  // Provides the DLC(s) information such as:
  // id, name, description, used_bytes_on_disk. (reference
  // `dlcservice::DlcsWithContent` proto for complete details)
  virtual void GetExistingDlcs(GetExistingDlcsCallback callback) = 0;

  // During testing, can be used to mimic signals received back from dlcservice.
  virtual void DlcStateChangedForTest(dbus::Signal* signal) = 0;

  // Adds an observer instance to the observers list to listen on changes like
  // DLC state change, etc.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer from observers list.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Creates and initializes the global instance. `bus` must not be nullptr.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be nullptr if not initialized.
  static DlcserviceClient* Get();

  DlcserviceClient(const DlcserviceClient&) = delete;
  DlcserviceClient& operator=(const DlcserviceClient&) = delete;

 protected:
  friend class DlcserviceClientTest;

  // Initialize/Shutdown should be used instead.
  DlcserviceClient();
  virtual ~DlcserviceClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_
