// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"

#include "content/public/browser/navigation_handle.h"

namespace multistep_filter {

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(FilterInitiatedNavigationMarker);

}  // namespace multistep_filter
