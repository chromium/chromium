// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_

#include <map>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash {

// A fake implementation of DlcserviceClient.
class COMPONENT_EXPORT(DLCSERVICE_CLIENT) FakeDlcserviceClient
    : public DlcserviceClient {
 public:
  FakeDlcserviceClient();
  ~FakeDlcserviceClient() override;

  // DlcserviceClient:
  void Install(const dlcservice::InstallRequest& install_request,
               InstallCallback callback,
               ProgressCallback progress_callback) override;
  // Uninstalling disables the DLC.
  void Uninstall(const std::string& dlc_id,
                 UninstallCallback callback) override;
  // Purging removes the DLC entirely from disk.
  void Purge(const std::string& dlc_id, PurgeCallback callback) override;
  void GetDlcState(const std::string& dlc_id,
                   GetDlcStateCallback callback) override;
  void GetExistingDlcs(GetExistingDlcsCallback callback) override;
  void DlcStateChangedForTest(dbus::Signal* signal) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void NotifyObserversForTest(const dlcservice::DlcState& dlc_state);

  // Setters:

  // This error will be returned by default (i.e. when there are no errors
  // queued by set_install_errors().
  void set_install_error(std::string_view err) { install_err_ = err; }

  // Set a list of errors which will be returned (in queue order). When there
  // are no errors left the default error set by set_install_error() will be
  // returned repeatedly.
  void set_install_errors(base::circular_deque<std::string> errs) {
    extra_install_errs_ = std::move(errs);
  }

  // When `true`, only record DLC `Install` calls that return `kErrorNone` in
  // `dlcs_with_content_`. Otherwise, record all DLC `Install` calls as
  // successful, even when it returns an error.
  void set_skip_adding_dlc_info_on_error(bool skip) {
    skip_adding_dlc_info_on_error_ = skip;
  }

  void set_install_root_path(std::string_view path) {
    install_root_path_ = path;
  }

  void set_uninstall_error(std::string_view err) { uninstall_err_ = err; }

  void set_purge_error(std::string_view err) { purge_err_ = err; }

  void set_get_existing_dlcs_error(std::string_view err) {
    get_existing_dlcs_err_ = err;
  }

  void set_dlcs_with_content(
      const dlcservice::DlcsWithContent& dlcs_with_content) {
    dlcs_with_content_ = dlcs_with_content;
  }

  void set_get_dlc_state_error(std::string_view id, std::string_view err) {
    get_dlc_state_errors_[std::string(id)] = err;
  }

  void set_dlc_state(std::string_view id,
                     const dlcservice::DlcState& dlc_state) {
    dlc_states_[std::string(id)] = dlc_state;
  }

 private:
  std::string GetInstallError();

  std::string install_err_ = dlcservice::kErrorNone;
  base::circular_deque<std::string> extra_install_errs_;
  bool skip_adding_dlc_info_on_error_ = false;
  std::string uninstall_err_ = dlcservice::kErrorNone;
  std::string purge_err_ = dlcservice::kErrorNone;
  std::string get_existing_dlcs_err_ = dlcservice::kErrorNone;
  std::string install_root_path_;
  dlcservice::DlcsWithContent dlcs_with_content_;
  std::map<std::string, std::string, std::less<>> get_dlc_state_errors_;
  std::map<std::string, dlcservice::DlcState, std::less<>> dlc_states_;

  // A list of observers that are listening on state changes, etc.
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DLCSERVICE_FAKE_DLCSERVICE_CLIENT_H_
