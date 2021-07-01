// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies_test_util.h"

namespace safe_browsing {

FileTypePoliciesTestOverlay::FileTypePoliciesTestOverlay()
    : orig_config_(new DownloadFileTypeConfig()) {
  // Make a copy of the global config so we can put it back later.
  // Swap out, copy, swap back in.
  SwapConfig(orig_config_);
  std::unique_ptr<DownloadFileTypeConfig> copy_config = DuplicateConfig();
  SwapConfig(copy_config);
}

FileTypePoliciesTestOverlay::~FileTypePoliciesTestOverlay() {
  SwapConfig(orig_config_);
}

void FileTypePoliciesTestOverlay::SwapConfig(
    std::unique_ptr<DownloadFileTypeConfig>& new_config) const {
  FileTypePolicies::GetInstance()->SwapConfig(new_config);
}

std::unique_ptr<DownloadFileTypeConfig>
FileTypePoliciesTestOverlay::DuplicateConfig() const {
  std::unique_ptr<DownloadFileTypeConfig> new_config(
      new DownloadFileTypeConfig());
  // Deep copy
  new_config->CopyFrom(*orig_config_);
  return new_config;
}

}  // namespace safe_browsing
