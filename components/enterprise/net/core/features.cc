// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/features.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

namespace enterprise_net {

BASE_FEATURE(kEnableDynamicRouteFetching, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr size_t kDefaultPvdConfigMaxSizeBytes = 3 * 1024 * 1024;

const base::FeatureParam<int> kPvdConfigMaxSizeBytesParam{
    &kEnableDynamicRouteFetching, "pvd_config_max_size_bytes",
    static_cast<int>(kDefaultPvdConfigMaxSizeBytes)};

bool IsDynamicRouteFetchingEnabled() {
  return base::FeatureList::IsEnabled(kEnableDynamicRouteFetching);
}

size_t GetPvdConfigMaxSizeBytes() {
  int size = kPvdConfigMaxSizeBytesParam.Get();
  return size > 0 ? static_cast<size_t>(size) : kDefaultPvdConfigMaxSizeBytes;
}

}  // namespace enterprise_net

#endif  // BUILDFLAG(ENTERPRISE_PROXY)
