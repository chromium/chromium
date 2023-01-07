// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_MONITOR_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_MONITOR_H_

#include <memory>

#include "base/functional/callback_forward.h"

// An abstract base for an object that monitors the browser's installation for
// updates.
class InstalledVersionMonitor {
 public:
  // A callback run to indicate either a monitored change or an error. The
  // single boolean argument is true in case of error, or false in case of a
  // change.
  using Callback = base::RepeatingCallback<void(bool)>;

  // Returns a new instance.
  static std::unique_ptr<InstalledVersionMonitor> Create();

  InstalledVersionMonitor(const InstalledVersionMonitor&) = delete;
  InstalledVersionMonitor& operator=(const InstalledVersionMonitor&) = delete;
  virtual ~InstalledVersionMonitor() = default;

  // Starts the monitor; may only be called once per instance. |callback| will
  // be run zero or more times with a false argument to report changes in the
  // installation, and at most one time with a true argument to report an error.
  // In case of error, no further notifications will be made. There is no
  // guarantee that |callback| will not be run after this instance is destroyed.
  virtual void Start(Callback callback) = 0;

 protected:
  InstalledVersionMonitor() = default;
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_MONITOR_H_
