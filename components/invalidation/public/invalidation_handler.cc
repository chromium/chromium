// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/invalidation_handler.h"

namespace invalidation {

void InvalidationHandler::OnInvalidatorClientIdChange(
    const std::string& client_id) {}

bool InvalidationHandler::IsPublicTopic(const Topic& topic) const {
  return false;
}

}  // namespace invalidation
