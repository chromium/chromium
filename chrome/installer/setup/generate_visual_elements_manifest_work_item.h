// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_GENERATE_VISUAL_ELEMENTS_MANIFEST_WORK_ITEM_H_
#define CHROME_INSTALLER_SETUP_GENERATE_VISUAL_ELEMENTS_MANIFEST_WORK_ITEM_H_

#include "base/files/file_path.h"
#include "base/version.h"
#include "chrome/installer/util/work_item.h"

namespace installer {

// Generates a chrome.VisualElementsManifest.xml in `target_dir` referencing the
// assets in its `version`\VisualElements directory.
class GenerateVisualElementsManifestWorkItem : public WorkItem {
 public:
  explicit GenerateVisualElementsManifestWorkItem(
      const base::FilePath& target_dir,
      const base::Version& version)
      : target_dir_(target_dir), version_(version) {}

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  const base::FilePath target_dir_;
  const base::Version version_;
  bool succeeded_ = false;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_GENERATE_VISUAL_ELEMENTS_MANIFEST_WORK_ITEM_H_
