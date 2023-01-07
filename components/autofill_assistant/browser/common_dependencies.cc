// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/common_dependencies.h"

namespace autofill_assistant {

CommonDependencies::~CommonDependencies() = default;

bool CommonDependencies::IsAllowedForMachineLearning() const {
  return true;
}

bool CommonDependencies::GetMakeSearchesAndBrowsingBetterEnabled() const {
  return false;
}

bool CommonDependencies::GetMetricsReportingEnabled() const {
  return false;
}

}  // namespace autofill_assistant
