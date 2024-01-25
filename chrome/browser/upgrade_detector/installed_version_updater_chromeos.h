// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_UPDATER_CHROMEOS_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_UPDATER_CHROMEOS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

class BuildState;

// Observes the system UpdateEngineClient for updates that require a device
// restart. Update information is pushed to the given BuildState as it happens.
class InstalledVersionUpdater : public ash::UpdateEngineClient::Observer {
 public:
  explicit InstalledVersionUpdater(BuildState* build_state);
  InstalledVersionUpdater(const InstalledVersionUpdater&) = delete;
  InstalledVersionUpdater& operator=(const InstalledVersionUpdater&) = delete;
  ~InstalledVersionUpdater() override;

  // ash::UpdateEngineClient::Observer:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

 private:
  // A callback run upon fetching either the current or target channel name
  // following a rollback.
  void OnChannel(bool is_current_channel, const std::string& channel_name);

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ptr<BuildState> build_state_;
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_UPDATER_CHROMEOS_H_
