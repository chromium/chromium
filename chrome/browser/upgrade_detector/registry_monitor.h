// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_REGISTRY_MONITOR_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_REGISTRY_MONITOR_H_

#include "base/functional/callback.h"
#include "base/win/registry.h"
#include "chrome/browser/upgrade_detector/installed_version_monitor.h"

// A monitor of installs on Windows that watches for changes in the browser's
// Clients registry key.
class RegistryMonitor final : public InstalledVersionMonitor {
 public:
  explicit RegistryMonitor(base::win::RegKey key);
  ~RegistryMonitor() override;

  // InstalledVersionMonitor:
  void Start(Callback on_change_callback) override;

 private:
  void StartWatching();
  void OnClientsKeyChanged();

  base::win::RegKey clients_key_;
  Callback on_change_callback_;
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_REGISTRY_MONITOR_H_
