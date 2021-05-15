// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_TEST_MOCK_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_UPDATES_TEST_MOCK_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_

#include "chrome/browser/updates/update_notification_service_bridge.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updates {
namespace test {

class MockUpdateNotificationServiceBridge
    : public UpdateNotificationServiceBridge {
 public:
  MockUpdateNotificationServiceBridge();
  ~MockUpdateNotificationServiceBridge();

  MOCK_METHOD1(UpdateLastShownTimeStamp, void(base::Time timestamp));
  MOCK_METHOD0(GetLastShownTimeStamp, absl::optional<base::Time>());
  MOCK_METHOD1(UpdateThrottleInterval, void(base::TimeDelta interval));
  MOCK_METHOD0(GetThrottleInterval, absl::optional<base::TimeDelta>());
  MOCK_METHOD1(UpdateNegativeActionCount, void(int count));
  MOCK_METHOD0(GetNegativeActionCount, int());
  MOCK_METHOD1(LaunchChromeActivity, void(int state));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUpdateNotificationServiceBridge);
};

}  // namespace test
}  // namespace updates

#endif  // CHROME_BROWSER_UPDATES_TEST_MOCK_UPDATE_NOTIFICATION_SERVICE_BRIDGE_H_
