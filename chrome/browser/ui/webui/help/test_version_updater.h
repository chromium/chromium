// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_TEST_VERSION_UPDATER_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_TEST_VERSION_UPDATER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

// A very simple VersionUpdater implementation that immediately invokes the
// StatusCallback with predefined parameters. Since this is only used for
// testing, only the parts of the interface that are needed for testing have
// been implemented.
class TestVersionUpdater : public VersionUpdater {
 public:
  TestVersionUpdater();
  ~TestVersionUpdater() override;

  void CheckForUpdate(const StatusCallback& callback,
                      const PromoteCallback&) override;

  void SetReturnedStatus(Status status) { status_ = status; }

// VersionUpdater implementation:
#if defined(OS_MACOSX)
  void PromoteUpdater() const override {}
#endif
#if defined(OS_CHROMEOS)
  void SetChannel(const std::string& channel,
                  bool is_powerwash_allowed) override {}
  void GetChannel(bool get_current_channel,
                  const ChannelCallback& callback) override {}
  void GetEolInfo(EolInfoCallback callback) override {}
  void SetUpdateOverCellularOneTimePermission(const StatusCallback& callback,
                                              const std::string& update_version,
                                              int64_t update_size) override {}
#endif

 private:
  Status status_ = Status::UPDATED;
  int progress_ = 0;
  bool rollback_ = false;
  std::string version_;
  int64_t update_size_ = 0;
  base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(TestVersionUpdater);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_TEST_VERSION_UPDATER_H_
