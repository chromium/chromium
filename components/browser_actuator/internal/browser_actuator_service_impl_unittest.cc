// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

TEST(BrowserActuatorServiceImplTest, IsInitialized) {
  BrowserActuatorServiceImpl service;
  EXPECT_TRUE(service.IsInitialized());
}

}  // namespace
}  // namespace browser_actuator
