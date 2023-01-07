// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/system/reboot/reboot_util.h"

#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

// Ensure that we can call RebootNow during a test without crashing
// and that it properly keeps track of the reboot source.
TEST(RebootUtil, CaptureReboot) {
  base::MockCallback<RebootUtil::RebootCallback> callback;
  RebootUtil::SetRebootCallbackForTest(callback.Get());
  EXPECT_CALL(callback, Run(RebootShlib::RebootSource::FORCED))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(RebootUtil::RebootNow(RebootShlib::RebootSource::FORCED));
  RebootUtil::ClearRebootCallbackForTest();
}

}  // namespace chromecast
