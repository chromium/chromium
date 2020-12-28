// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_BASIC_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_BASIC_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

// Bare bones implementation just checks if a new version is ready.
class VersionUpdaterBasic : public VersionUpdater {
 public:
  // VersionUpdater implementation.
  void CheckForUpdate(StatusCallback callback, PromoteCallback) override;

 protected:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterBasic() {}
  ~VersionUpdaterBasic() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterBasic);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_BASIC_H_
