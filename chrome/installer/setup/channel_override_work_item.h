// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_CHANNEL_OVERRIDE_WORK_ITEM_H_
#define CHROME_INSTALLER_SETUP_CHANNEL_OVERRIDE_WORK_ITEM_H_

#include <optional>

#include "chrome/installer/util/additional_parameters.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem that, when run, will change the browser's "ap" value so that the
// update channel identified by "ap" matches the current channel. No change is
// made if the channels match. The replacements for Google Chrome are as
// follows:
//
// Channel   |  No Arch     |  x64                  |  x86
// ----------+--------------+-----------------------+-----------------------
// stable    |  ""          |  "x64-stable"         |  "stable-arch_x86"
// extended  |  "extended"  |  "extended-arch_x64"  |  "extended-arch_x86"
// beta      |  "1.1-beta"  |  "1.1-beta-arch_x64"  |  "1.1-beta-arch_x86"
// dev       |  "2.0-dev"   |  "2.0-dev-arch_x64"   |  "2.0-dev-arch_x86"
//
// The architecture-specific replacements are used only when "ap" already
// contains an architecture specification. In all cases, the current
// architecture is used.
class ChannelOverrideWorkItem : public WorkItem {
 public:
  ChannelOverrideWorkItem();
  ChannelOverrideWorkItem(const ChannelOverrideWorkItem&) = delete;
  ChannelOverrideWorkItem& operator=(const ChannelOverrideWorkItem&) = delete;
  ~ChannelOverrideWorkItem() override;

 private:
  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // The original value to be used in rollback. Only valid when a change has
  // been made.
  std::optional<installer::AdditionalParameters> original_ap_;
};

#endif  // CHROME_INSTALLER_SETUP_CHANNEL_OVERRIDE_WORK_ITEM_H_
