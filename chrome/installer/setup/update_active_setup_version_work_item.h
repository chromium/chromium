// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_UPDATE_ACTIVE_SETUP_VERSION_WORK_ITEM_H_
#define CHROME_INSTALLER_SETUP_UPDATE_ACTIVE_SETUP_VERSION_WORK_ITEM_H_

#include <string>

#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/work_item.h"

// A WorkItem that updates (or installs if not present) the Active Setup
// "Version" field in the registry. Optionally bumping the SELECTIVE_TRIGGER
// component on demand. This WorkItem is only viable on machine-wide installs.
class UpdateActiveSetupVersionWorkItem : public WorkItem {
 public:
  // The components of the Active Setup Version entry, in order.
  enum VersionComponent {
    // The major version.
    MAJOR,
    // Unused component, always 0 for now.
    UNUSED1,
    // A component used to selectively trigger Active Setup based on local
    // factors such as an OS upgrade.
    SELECTIVE_TRIGGER,
    // Unused component, always 0 for now.
    UNUSED2,
  };

  // The operation to be performed by this UpdateActiveSetupVersionWorkItem.
  enum Operation {
    // Update (or install if not present) the Active Setup "Version" in the
    // registry.
    UPDATE,
    // Also bump the SELECTIVE_TRIGGER component on top of updating the version
    // (will default to 1 if the version was absent or invalid).
    UPDATE_AND_BUMP_SELECTIVE_TRIGGER,
  };

  // Constructs an UpdateActiveSetupVersionWorkItem that will perform
  // |operation| on the |active_setup_path| key in the registry. This key needs
  // to exist when this WorkItem is ran.
  UpdateActiveSetupVersionWorkItem(const std::wstring& active_setup_path,
                                   Operation operation);

  UpdateActiveSetupVersionWorkItem(const UpdateActiveSetupVersionWorkItem&) =
      delete;
  UpdateActiveSetupVersionWorkItem& operator=(
      const UpdateActiveSetupVersionWorkItem&) = delete;

 private:
  // WorkItem:
  bool DoImpl() override;
  void RollbackImpl() override;

  // Returns the updated Active Setup version to be used based on the
  // |existing_version|.
  std::wstring GetUpdatedActiveSetupVersion(
      const std::wstring& existing_version);

  // The underlying WorkItem re-used to operate forward and backward on the
  // registry.
  SetRegValueWorkItem set_reg_value_work_item_;

  // The Operation to be performed by this WorkItem when executed.
  const Operation operation_;
};

#endif  // CHROME_INSTALLER_SETUP_UPDATE_ACTIVE_SETUP_VERSION_WORK_ITEM_H_
