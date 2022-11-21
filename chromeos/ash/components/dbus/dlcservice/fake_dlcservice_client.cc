// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeDlcserviceClient::FakeDlcserviceClient() = default;

FakeDlcserviceClient::~FakeDlcserviceClient() = default;

void FakeDlcserviceClient::Install(
    const dlcservice::InstallRequest& install_request,
    InstallCallback callback,
    ProgressCallback progress_callback) {
  VLOG(1) << "Requesting to install DLC(s).";
  const std::string& id = install_request.id();
  InstallResult install_result{
      .error = GetInstallError(),
      .dlc_id = id,
      .root_path = install_root_path_,
  };
  dlcs_with_content_.add_dlc_infos()->set_id(id);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(install_result)));
}

void FakeDlcserviceClient::Uninstall(const std::string& dlc_id,
                                     UninstallCallback callback) {
  VLOG(1) << "Requesting to uninstall DLC=" << dlc_id;
  for (auto iter = dlcs_with_content_.dlc_infos().begin();
       iter != dlcs_with_content_.dlc_infos().end();) {
    if (iter->id() != dlc_id) {
      iter++;
      continue;
    }
    iter = dlcs_with_content_.mutable_dlc_infos()->erase(iter);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), uninstall_err_));
}

void FakeDlcserviceClient::Purge(const std::string& dlc_id,
                                 PurgeCallback callback) {
  VLOG(1) << "Requesting to purge DLC=" << dlc_id;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), purge_err_));
}

void FakeDlcserviceClient::GetDlcState(const std::string& dlc_id,
                                       GetDlcStateCallback callback) {
  VLOG(1) << "Requesting to get DLC state of" << dlc_id;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_dlc_state_err_, dlc_state_));
}

void FakeDlcserviceClient::GetExistingDlcs(GetExistingDlcsCallback callback) {
  VLOG(1) << "Requesting to get existing DLC(s).";
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), get_existing_dlcs_err_,
                                dlcs_with_content_));
}

void FakeDlcserviceClient::DlcStateChangedForTest(dbus::Signal* signal) {
  NOTREACHED();
}

void FakeDlcserviceClient::NotifyObserversForTest(
    const dlcservice::DlcState& dlc_state) {
  // Notify all observers with the state change |dlc_state|.
  for (Observer& observer : observers_) {
    observer.OnDlcStateChanged(dlc_state);
  }
}

void FakeDlcserviceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeDlcserviceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::string FakeDlcserviceClient::GetInstallError() {
  if (extra_install_errs_.empty()) {
    return install_err_;
  }
  std::string ret(std::move(extra_install_errs_.front()));
  extra_install_errs_.pop_front();
  return ret;
}

}  // namespace ash
