// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_screen.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/display/display_observer.h"

using testing::_;
using testing::AllOf;
using testing::Property;

namespace chromecast {
namespace test {

namespace {
constexpr int64_t kMockDisplayId = 0xcafebabe;
}

using CastScreenTest = aura::test::AuraTestBase;

class MockDisplayObserver : public display::DisplayObserver {
 public:
  MOCK_METHOD2(OnDisplayMetricsChanged,
               void(const display::Display& display, uint32_t changed_metrics));
};

TEST_F(CastScreenTest, OverrideAndRestore) {
  MockDisplayObserver mock_display_observer;
  CastScreen screen;
  // Set up initial screen.
  screen.OnDisplayChanged(kMockDisplayId, 1.0,
                          display::Display::Rotation::ROTATE_0,
                          gfx::Rect(0, 0, 1920, 1080));

  EXPECT_CALL(mock_display_observer,
              OnDisplayMetricsChanged(
                  AllOf(Property(&display::Display::id, kMockDisplayId),
                        Property(&display::Display::device_scale_factor, 2.2),
                        Property(&display::Display::rotation,
                                 display::Display::Rotation::ROTATE_270)),
                  _));
  EXPECT_CALL(mock_display_observer,
              OnDisplayMetricsChanged(
                  AllOf(Property(&display::Display::id, kMockDisplayId),
                        Property(&display::Display::device_scale_factor, 1.0),
                        Property(&display::Display::rotation,
                                 display::Display::Rotation::ROTATE_0)),
                  _));
  screen.AddObserver(&mock_display_observer);
  screen.OverridePrimaryDisplaySettings(gfx::Rect(0, 0, 1000, 1000), 2.2,
                                        display::Display::Rotation::ROTATE_270);
  screen.RestorePrimaryDisplaySettings();
}

}  // namespace test
}  // namespace chromecast
