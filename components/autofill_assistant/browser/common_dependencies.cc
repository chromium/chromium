// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/common_dependencies.h"

namespace autofill_assistant {

CommonDependencies::~CommonDependencies() = default;

bool CommonDependencies::IsAllowedForMachineLearning(
    content::BrowserContext* browser_context) const {
  return true;
}

bool CommonDependencies::GetMakeSearchesAndBrowsingBetterEnabled(
    content::BrowserContext* browser_context) const {
  return false;
}

bool CommonDependencies::GetMetricsReportingEnabled(
    content::BrowserContext* browser_context) const {
  return false;
}

}  // namespace autofill_assistant
