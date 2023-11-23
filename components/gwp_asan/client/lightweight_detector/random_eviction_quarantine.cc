// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector/random_eviction_quarantine.h"

namespace gwp_asan::internal::lud {

template class SharedState<RandomEvictionQuarantine>;

}  // namespace gwp_asan::internal::lud
