// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_MOCK_VSYNC_TIMING_OBSERVER_H_
#define COMPONENTS_EXO_MOCK_VSYNC_TIMING_OBSERVER_H_

#include "components/exo/vsync_timing_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace exo {

class MockVSyncTimingObserver : public VSyncTimingManager::Observer {
 public:
  MockVSyncTimingObserver();
  ~MockVSyncTimingObserver() override;

  MOCK_METHOD(void,
              OnUpdateVSyncParameters,
              (base::TimeTicks timebase, base::TimeDelta interval),
              (override));
};

}  // namespace exo

#endif  // COMPONENTS_EXO_MOCK_VSYNC_TIMING_OBSERVER_H_
