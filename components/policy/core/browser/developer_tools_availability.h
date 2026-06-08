// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_DEVELOPER_TOOLS_AVAILABILITY_H_
#define COMPONENTS_POLICY_CORE_BROWSER_DEVELOPER_TOOLS_AVAILABILITY_H_

namespace policy {

// Developer tools availability as set by policy. The values must match the
// 'DeveloperToolsAvailability' policy definition.
enum class DeveloperToolsAvailability {
  // Default: Developer tools are allowed, except for policy-installed
  // extensions and, if this is a managed profile, component extensions.
  kDisallowedForForceInstalledExtensions = 0,
  // Developer tools allowed in all contexts.
  kAllowed = 1,
  // Developer tools disallowed in all contexts.
  kDisallowed = 2,
  // Maximal valid value for range checking.
  kMaxValue = kDisallowed
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_DEVELOPER_TOOLS_AVAILABILITY_H_
