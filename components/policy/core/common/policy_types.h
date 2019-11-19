// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_TYPES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_TYPES_H_

namespace policy {

// The scope of a policy flags whether it is meant to be applied to the current
// user or to the machine.  Note that this property pertains to the source of
// the policy and has no direct correspondence to the distinction between User
// Policy and Device Policy.
enum PolicyScope {
  // USER policies apply to sessions of the current user.
  POLICY_SCOPE_USER,

  // MACHINE policies apply to any users of the current machine.
  POLICY_SCOPE_MACHINE,
};

// The level of a policy determines its enforceability and whether users can
// override it or not. The values are listed in increasing order of priority.
enum PolicyLevel {
  // RECOMMENDED policies can be overridden by users. They are meant as a
  // default value configured by admins, that users can customize.
  POLICY_LEVEL_RECOMMENDED,

  // MANDATORY policies must be enforced and users can't circumvent them.
  POLICY_LEVEL_MANDATORY,
};

// The source of a policy indicates where its value is originating from. The
// sources are ordered by priority (with weakest policy first).
enum PolicySource {
  // The policy was set because we are running in an enterprise environment.
  POLICY_SOURCE_ENTERPRISE_DEFAULT,

  // The policy was set by a cloud source.
  POLICY_SOURCE_CLOUD,

  // The policy was set by an Active Directory source.
  POLICY_SOURCE_ACTIVE_DIRECTORY,

  // Any non-platform policy was overridden because we are running in a
  // public session or kiosk mode.
  POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE,

  // The policy was set by a platform source.
  POLICY_SOURCE_PLATFORM,

  // The policy was set by a cloud source that has higher priroity.
  POLICY_SOURCE_PRIORITY_CLOUD,

  // The policy coming from multiple sources and its value has been merged.
  POLICY_SOURCE_MERGED,

  // Number of source types. Has to be the last element.
  POLICY_SOURCE_COUNT
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_TYPES_H_
