// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_METRICS_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_METRICS_H_

namespace policy {

// This enum is used for recording the metrics. It must match the
// MachineLevelUserCloudPolicyEnrollmentResult in enums.xml and should not be
// reordered. |kMaxValue| must be assigned to the last entry of the enum.
enum class ChromeBrowserCloudManagementEnrollmentResult {
  kSuccess = 0,
  kFailedToFetch = 1,
  kFailedToStore = 2,
  kMaxValue = kFailedToStore,
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CHROME_BROWSER_CLOUD_MANAGEMENT_METRICS_H_
