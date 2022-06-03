// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_MOCK_BUILD_STATE_OBSERVER_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_MOCK_BUILD_STATE_OBSERVER_H_

#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockBuildStateObserver : public BuildStateObserver {
 public:
  MockBuildStateObserver();
  ~MockBuildStateObserver() override;
  MOCK_METHOD(void, OnUpdate, (const BuildState*), (override));
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_MOCK_BUILD_STATE_OBSERVER_H_
