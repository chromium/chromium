// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_MONITOR_LACROS_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_MONITOR_LACROS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/upgrade_detector/installed_version_monitor.h"
#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Observes browser version changes as a Crosapi client and triggers the
// appropriate callback.
class LacrosInstalledVersionMonitor final
    : public InstalledVersionMonitor,
      public crosapi::mojom::BrowserVersionObserver {
 public:
  explicit LacrosInstalledVersionMonitor(chromeos::LacrosService* service);
  ~LacrosInstalledVersionMonitor() override;

  // InstalledVersionMonitor:
  void Start(Callback callback) override;

  // crosapi::mojom::BrowserVersionObserver:
  void OnBrowserVersionInstalled(const std::string& version) override;

 private:
  const raw_ptr<chromeos::LacrosService> lacros_service_;

  Callback callback_;

  // Receives mojo messages from ash-chrome (under Streaming mode).
  mojo::Receiver<crosapi::mojom::BrowserVersionObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_MONITOR_LACROS_H_
