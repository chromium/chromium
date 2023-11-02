// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/test/mock_media_router.h"

#include <memory>

namespace media_router {

// static
std::unique_ptr<KeyedService> MockMediaRouter::Create(
    content::BrowserContext* context) {
  return std::make_unique<::testing::NiceMock<MockMediaRouter>>();
}

MockMediaRouter::MockMediaRouter() = default;

MockMediaRouter::~MockMediaRouter() = default;

}  // namespace media_router
