// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_BACKOFF_POLICY_H_
#define COMPONENTS_GCM_DRIVER_GCM_BACKOFF_POLICY_H_

#include "net/base/backoff_entry.h"

namespace gcm {

// Returns the backoff policy that applies to all GCM requests.
const net::BackoffEntry::Policy& GetGCMBackoffPolicy();

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_BACKOFF_POLICY_H_
