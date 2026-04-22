// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/activity_provider.h"

#include "components/record_replay/core/browser/activity_hardcoded_provider.h"

namespace record_replay {

// static
std::vector<std::unique_ptr<ActivityProvider>>
ActivityProvider::CreateProviders() {
  std::vector<std::unique_ptr<ActivityProvider>> providers;
  providers.push_back(std::make_unique<ActivityHardcodedProvider>());
  return providers;
}

}  // namespace record_replay
