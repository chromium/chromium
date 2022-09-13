// Copyright 2016 The Chromium Authors
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

FileTypePoliciesTestOverlay::FileTypePoliciesTestOverlay(
    FileTypePoliciesTestOverlay&&) = default;
FileTypePoliciesTestOverlay& FileTypePoliciesTestOverlay::operator=(
    FileTypePoliciesTestOverlay&&) = default;

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

FileTypePoliciesTestOverlay ScopedMarkAllFilesDangerousForTesting() {
  safe_browsing::FileTypePoliciesTestOverlay file_type_configuration;
  std::unique_ptr<safe_browsing::DownloadFileTypeConfig> fake_file_type_config =
      std::make_unique<safe_browsing::DownloadFileTypeConfig>();
  auto* file_type = fake_file_type_config->mutable_default_file_type();
  file_type->set_uma_value(-1);
  auto* platform_settings = file_type->add_platform_settings();
  platform_settings->set_danger_level(
      safe_browsing::DownloadFileType::DANGEROUS);
  file_type_configuration.SwapConfig(fake_file_type_config);
  return file_type_configuration;
}

}  // namespace safe_browsing
