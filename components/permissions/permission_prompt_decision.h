// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_DECISION_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_DECISION_H_

#include "components/permissions/permission_decision.h"
#include "components/permissions/resolvers/permission_prompt_options.h"

namespace permissions {

// Represents a permission decision on a prompt. These decisions are passed into
// the infrastructure in `PermissionRequest`. Specifically, the
// PermissionRequestManager forwards permission decisions by calling
// `PermissionRequest::PermissionGranted`,
// `PermissionRequest::PermissionDenied`, and `PermissionRequest::Cancelled`.
// These methods then execute the permission decided callback with the
// appropriate permission decision enum value, where the decision is used the
// determine and possibly persist the permission state resulting from the
// decision.
struct PermissionPromptDecision {
  bool operator==(const PermissionPromptDecision&) const = default;

  PermissionDecision overall_decision;
  PromptOptions prompt_options;
  bool is_final;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_PROMPT_DECISION_H_
