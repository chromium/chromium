// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_TEST_VERSION_UPDATER_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_TEST_VERSION_UPDATER_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

// A very simple VersionUpdater implementation that immediately invokes the
// StatusCallback with predefined parameters. Since this is only used for
// testing, only the parts of the interface that are needed for testing have
// been implemented.
class TestVersionUpdater : public VersionUpdater {
 public:
  TestVersionUpdater();

  TestVersionUpdater(const TestVersionUpdater&) = delete;
  TestVersionUpdater& operator=(const TestVersionUpdater&) = delete;

  ~TestVersionUpdater() override;

  void CheckForUpdate(StatusCallback callback, PromoteCallback) override;

  void SetReturnedStatus(Status status) { status_ = status; }

// VersionUpdater implementation:
#if BUILDFLAG(IS_MAC)
  void PromoteUpdater() override {}
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetChannel(const std::string& channel,
                  bool is_powerwash_allowed) override {}
  void GetChannel(bool get_current_channel, ChannelCallback callback) override {
  }
  void GetEolInfo(EolInfoCallback callback) override {}
  void ToggleFeature(const std::string& feature, bool enable) override {}
  void IsFeatureEnabled(const std::string& feature,
                        IsFeatureEnabledCallback callback) override {}
  bool IsManagedAutoUpdateEnabled() override;
  void SetUpdateOverCellularOneTimePermission(StatusCallback callback,
                                              const std::string& update_version,
                                              int64_t update_size) override {}
  void ApplyDeferredUpdate() override {}
#endif

 private:
  Status status_ = Status::UPDATED;
  int progress_ = 0;
  bool rollback_ = false;
  bool powerwash_ = false;
  std::string version_;
  int64_t update_size_ = 0;
  std::u16string message_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_TEST_VERSION_UPDATER_H_
