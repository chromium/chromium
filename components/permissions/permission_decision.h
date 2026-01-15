// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_DECISION_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_DECISION_H_

// Represents a permission decision on a prompt. Should be mostly passed around
// as part of permissions::PermissionPromptDecision.
enum class PermissionDecision {
  kAllow = 0,
  kAllowThisTime = 1,
  kDeny = 2,
  kNone = 3,  // No decision made / cancelled.
  kMaxValue = kNone,
};

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_DECISION_H_
