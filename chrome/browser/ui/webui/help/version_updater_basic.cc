// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater.h"

#include <memory>
#include <string>

#include "chrome/browser/upgrade_detector/upgrade_detector.h"

namespace {

// Bare bones implementation just checks if a new version is ready.
class VersionUpdaterBasic : public VersionUpdater {
 public:
  VersionUpdaterBasic(const VersionUpdaterBasic&) = delete;
  VersionUpdaterBasic& operator=(const VersionUpdaterBasic&) = delete;
  VersionUpdaterBasic() = default;
  ~VersionUpdaterBasic() override = default;

  // VersionUpdater implementation.
  void CheckForUpdate(StatusCallback callback, PromoteCallback) override {
    const Status status = UpgradeDetector::GetInstance()->is_upgrade_available()
                              ? NEARLY_UPDATED
                              : DISABLED;
    callback.Run(status, 0, false, false, std::string(), 0, std::u16string());
  }
};

}  // namespace

std::unique_ptr<VersionUpdater> VersionUpdater::Create(
    content::WebContents* web_contents) {
  return std::make_unique<VersionUpdaterBasic>();
}
