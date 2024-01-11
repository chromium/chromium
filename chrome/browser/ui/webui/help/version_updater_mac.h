// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_MAC_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_MAC_H_

#include "chrome/browser/ui/webui/help/version_updater.h"

// macOS implementation of version update functionality, used by the WebUI
// About/Help page.
class VersionUpdaterMac : public VersionUpdater {
 public:
  VersionUpdaterMac(const VersionUpdaterMac&) = delete;
  VersionUpdaterMac& operator=(const VersionUpdaterMac&) = delete;

  // VersionUpdater implementation.
  void CheckForUpdate(StatusCallback status_callback,
                      PromoteCallback promote_callback) override;
  void PromoteUpdater() override;

 protected:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterMac();
  ~VersionUpdaterMac() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_MAC_H_
