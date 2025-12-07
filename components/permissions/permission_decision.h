// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_DECISION_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_DECISION_H_

// Represents a permission decision on a prompt. These decisions are passed into
// the infrastructure in `PermissionRequest`. Specifically, the
// PermissionRequestManager forwards permission decisions by calling
// `PermissionRequest::PermissionGranted`,
// `PermissionRequest::PermissionDenied`, and `PermissionRequest::Cancelled`.
// These methods then execute the permission decided callback with the
// appropriate permission decision enum value, where the decision is used the
// determine and possibly persist the permission state resulting from the
// decision.
enum class PermissionDecision {
  kAllow = 0,
  kAllowThisTime = 1,
  kDeny = 2,
  kNone = 3,  // No decision made / cancelled.
  kMaxValue = kNone,
};

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_DECISION_H_
