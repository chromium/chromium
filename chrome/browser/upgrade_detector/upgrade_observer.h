// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_OBSERVER_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_OBSERVER_H_

class UpgradeObserver {
 public:
  // Triggered when a software update is downloaded but deferred.
  virtual void OnUpdateDeferred(bool use_notification) {}

  // Triggered when a software update is available, but downloading requires
  // user's agreement as current connection is cellular.
  virtual void OnUpdateOverCellularAvailable() {}

  // Triggered when the user's one time permission on update over cellular
  // connection has been granted.
  virtual void OnUpdateOverCellularOneTimePermissionGranted() {}

  // Triggered when Chrome believes an update has been installed and available
  // for long enough with the user shutting down to let it take effect, or
  // following a change to the thresholds that move the UpgradeDetector through
  // the low, elevated, and high annoyance levels. No details are expected.
  virtual void OnUpgradeRecommended() {}

  // Triggered when a critical update has been installed. No details are
  // expected.
  virtual void OnCriticalUpgradeInstalled() {}

  // Triggered when the current install is outdated. No details are expected.
  virtual void OnOutdatedInstall() {}

  // Triggered when the current install is outdated and auto-update (AU) is
  // disabled. No details are expected.
  virtual void OnOutdatedInstallNoAutoUpdate() {}

  // Triggered when a request to override the relaunch notification style to
  // required or reset the overridden style is received.
  virtual void OnRelaunchOverriddenToRequired(bool overridden) {}

 protected:
  virtual ~UpgradeObserver() {}
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_UPGRADE_OBSERVER_H_
