// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_H_
#define COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_H_

#include "base/callback.h"
#include "base/optional.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace url {
class Origin;
}  // namespace url

namespace background_sync {

// Allows the component embedder to override the behavior of Background Sync
// component.
class BackgroundSyncDelegate {
 public:
  virtual ~BackgroundSyncDelegate() = default;

  // Gets the source_ID to log the UKM event for, and calls |callback| with that
  // source_id, or with base::nullopt if UKM recording is not allowed.
  virtual void GetUkmSourceId(
      const url::Origin& origin,
      base::OnceCallback<void(base::Optional<ukm::SourceId>)> callback) = 0;
};

}  // namespace background_sync

#endif  // COMPONENTS_BACKGROUND_SYNC_BACKGROUND_SYNC_DELEGATE_H_
