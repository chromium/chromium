// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/policy/headless_mode_policy_handler.h"

#include "components/headless/policy/headless_mode_policy.h"
#include "components/headless/policy/headless_mode_prefs.h"
#include "components/policy/policy_constants.h"

namespace headless {

HeadlessModePolicyHandler::HeadlessModePolicyHandler()
    : policy::IntRangePolicyHandler(
          policy::key::kHeadlessMode,
          headless::prefs::kHeadlessMode,
          static_cast<int>(HeadlessModePolicy::HeadlessMode::kMinValue),
          static_cast<int>(HeadlessModePolicy::HeadlessMode::kMaxValue),
          false) {}

HeadlessModePolicyHandler::~HeadlessModePolicyHandler() = default;

}  // namespace headless
