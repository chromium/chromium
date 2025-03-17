// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_PARENT_ACCESS_TEST_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_PARENT_ACCESS_TEST_UTILS_H_

#include <string>

namespace supervised_user {

// Helper method that returns a base64 encoded approval result for local web
// approvals callback.
std::string CreatePacpApprovalResult();

// Helper method that returns a base64 encoded resize result for local web
// approvals callback.
std::string CreatePacpResizeResult();

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_TEST_SUPPORT_PARENT_ACCESS_TEST_UTILS_H_
