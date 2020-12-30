// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdateServiceInternalImplTest, UpdateCheckJitter) {
  for (int i = 0; i < 100; ++i) {
    base::TimeDelta jitter = UpdateCheckJitter();
    EXPECT_GE(jitter, base::TimeDelta::FromSeconds(0));
    EXPECT_LT(jitter, base::TimeDelta::FromSeconds(60));
  }
}

}  // namespace updater
