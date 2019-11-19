// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator.h"

namespace syncer {

Invalidator::Invalidator() {
}

Invalidator::~Invalidator() {
}

bool Invalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                      const Topics& ids) {
  NOTREACHED();
  return false;
}
}  // namespace syncer
