// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/activity_hardcoded_provider.h"

#include "url/gurl.h"

namespace record_replay {

ActivityHardcodedProvider::ActivityHardcodedProvider() = default;
ActivityHardcodedProvider::~ActivityHardcodedProvider() = default;

void ActivityHardcodedProvider::ShouldOfferActivity(
    const GURL& url,
    base::OnceCallback<
        void(std::optional<ActivityDiscoveryService::AutomationMetadata>)>
        callback) {
  std::move(callback).Run(std::nullopt);
}

}  // namespace record_replay
