// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_FEATURES_H_

#include <cstddef>

#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace enterprise_net {

// Feature flag controlling dynamic route fetching.
BASE_DECLARE_FEATURE(kEnableDynamicRouteFetching);

// Feature param for the maximum size limit (in bytes) for Provisioning Domain
// configuration downloads. Defaults to 3 MiB.
extern const base::FeatureParam<int> kPvdConfigMaxSizeBytesParam;

// Return true if dynamic route fetching is enabled.
bool IsDynamicRouteFetchingEnabled();

// Returns the maximum allowed size in bytes for Provisioning Domain
// configuration downloads.
size_t GetPvdConfigMaxSizeBytes();

}  // namespace enterprise_net

#endif  // BUILDFLAG(ENTERPRISE_PROXY)

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_FEATURES_H_
