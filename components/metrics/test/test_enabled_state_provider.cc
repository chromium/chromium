// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test/test_enabled_state_provider.h"

namespace metrics {

bool TestEnabledStateProvider::IsConsentGiven() const {
  return consent_;
}

bool TestEnabledStateProvider::IsReportingEnabled() const {
  return enabled_;
}

}  // namespace metrics
