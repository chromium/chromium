// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_basic.h"

#include <string>

#include "chrome/browser/upgrade_detector/upgrade_detector.h"

void VersionUpdaterBasic::CheckForUpdate(StatusCallback status_callback,
                                         PromoteCallback) {
  const Status status = UpgradeDetector::GetInstance()->is_upgrade_available()
                            ? NEARLY_UPDATED
                            : DISABLED;
  status_callback.Run(status, 0, false, false, std::string(), 0,
                      std::u16string());
}

VersionUpdater* VersionUpdater::Create(content::WebContents* web_contents) {
  return new VersionUpdaterBasic;
}
