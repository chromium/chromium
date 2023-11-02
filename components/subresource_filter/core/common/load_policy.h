// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_LOAD_POLICY_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_LOAD_POLICY_H_

namespace subresource_filter {

// Represents the value returned by the DocumentSubresourceFilter corresponding
// to a resource load. Ordered by in increasing severity.
enum class LoadPolicy {
  // Policy for resources that are specifically allowed by the filter. Should be
  // treated the same as ALLOW.
  EXPLICITLY_ALLOW,
  ALLOW,
  // Policy for disallowed resources when the filter is running in dry run mode.
  WOULD_DISALLOW,
  DISALLOW,
};

// Returns the stricter of the two load policies, as determined by the order
// of the LoadPolicy enum.
LoadPolicy MoreRestrictiveLoadPolicy(LoadPolicy a, LoadPolicy b);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_LOAD_POLICY_H_
