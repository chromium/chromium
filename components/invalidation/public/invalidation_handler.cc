// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation_handler.h"

namespace syncer {

InvalidationHandler::InvalidationHandler() {
}

InvalidationHandler::~InvalidationHandler() {
}

bool InvalidationHandler::IsPublicTopic(const Topic& topic) const {
  return false;
}

}  // namespace syncer
