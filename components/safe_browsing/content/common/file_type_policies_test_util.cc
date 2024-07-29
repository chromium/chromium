// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies_test_util.h"

#include "base/values.h"

namespace safe_browsing {

namespace {

base::Value::List CreateStringListValueForTest(
    const std::vector<std::string>& items) {
  base::Value::List list;
  for (const auto& item : items) {
    list.Append(item);
  }
  return list;
}

}  // namespace

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

base::Value::Dict CreateNotDangerousOverridePolicyEntryForTesting(
    const std::string& extension,
    const std::vector<std::string>& domains) {
  base::Value::Dict out;
  out.Set("file_extension", base::Value{extension});
  out.Set("domains", CreateStringListValueForTest(domains));
  return out;
}

}  // namespace safe_browsing
