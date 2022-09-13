// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_TEST_UTIL_H_
#define COMPONENTS_EXO_SURFACE_TEST_UTIL_H_

#include "components/exo/surface_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace exo {

class SurfaceObserverForTest : public SurfaceObserver {
 public:
  SurfaceObserverForTest();
  SurfaceObserverForTest(const SurfaceObserverForTest&) = delete;
  SurfaceObserverForTest& operator=(const SurfaceObserverForTest&) = delete;
  ~SurfaceObserverForTest() override;

  // SurfaceObserver overrides
  void OnSurfaceDestroying(Surface* surface) override {}

  void OnWindowOcclusionChanged(Surface* surface) override {
    num_occlusion_changes_++;
  }

  int num_occlusion_changes() const { return num_occlusion_changes_; }

  MOCK_METHOD(void, ThrottleFrameRate, (bool on), (override));

 private:
  int num_occlusion_changes_ = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_TEST_UTIL_H_
