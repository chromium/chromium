// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/invalidation_interface.h"

namespace syncer {

bool InvalidationInterface::LessThanByVersion(const InvalidationInterface& a,
                                              const InvalidationInterface& b) {
  if (a.IsUnknownVersion() && !b.IsUnknownVersion())
    return true;

  if (!a.IsUnknownVersion() && b.IsUnknownVersion())
    return false;

  if (a.IsUnknownVersion() && b.IsUnknownVersion())
    return false;

  return a.GetVersion() < b.GetVersion();
}

InvalidationInterface::InvalidationInterface() = default;

InvalidationInterface::~InvalidationInterface() = default;

}  // namespace syncer
