// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_ORIGIN_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_ORIGIN_H_

namespace unexportable_keys {

// An enum describing where the scheduling of a background task originates from.
enum class BackgroundTaskOrigin {
  kRefreshTokenBinding = 0,
  kDeviceBoundSessionCredentials = 1,
  kDeviceBoundSessionCredentialsPrototype = 2,
  kOrphanedKeyGarbageCollection = 3,
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_BACKGROUND_TASK_ORIGIN_H_
