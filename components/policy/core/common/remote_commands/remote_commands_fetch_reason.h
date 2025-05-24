// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_FETCH_REASON_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_FETCH_REASON_H_

namespace policy {

enum class RemoteCommandsFetchReason {
  kTest,
  kStartup,
  kUploadExecutionResults,
  kUserRequest,
  kInvalidation,
};
}

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_FETCH_REASON_H_
