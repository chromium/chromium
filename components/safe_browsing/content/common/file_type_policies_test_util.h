// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_TEST_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_TEST_UTIL_H_

#include "base/values.h"
#include "components/safe_browsing/content/common/file_type_policies.h"

namespace safe_browsing {

// This is a test fixture for modifying the proto with FileTypePolicies.
// While an object of this class is in scope, it will cause callers
// of FileTypePolicies::GetInstance() to see the modified list.
// When it goes out of scope, future callers will get the original list.
//
// Example:
//   FileTypePoliciesTestOverlay overlay_;
//   std::unique_ptr<DownloadFileTypesConfig> cfg =
//       overlay_.DuplicateConfig();
//   cfg.set_sampled_ping_probability(1.0);
//   overlay_.SwapConfig(cfg);
//   ...
class FileTypePoliciesTestOverlay {
 public:
  FileTypePoliciesTestOverlay();
  ~FileTypePoliciesTestOverlay();
  FileTypePoliciesTestOverlay(FileTypePoliciesTestOverlay&&);
  FileTypePoliciesTestOverlay& operator=(FileTypePoliciesTestOverlay&&);

  // Swaps the contents bewtween the existing config and |new_config|.
  void SwapConfig(std::unique_ptr<DownloadFileTypeConfig>& new_config) const;

  // Return a new copy of the original config.
  std::unique_ptr<DownloadFileTypeConfig> DuplicateConfig() const;

 private:
  std::unique_ptr<DownloadFileTypeConfig> orig_config_;
};

FileTypePoliciesTestOverlay ScopedMarkAllFilesDangerousForTesting();

base::Value::Dict CreateNotDangerousOverridePolicyEntryForTesting(
    const std::string& extension,
    const std::vector<std::string>& domains);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_FILE_TYPE_POLICIES_TEST_UTIL_H_
