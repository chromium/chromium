// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/arc_input_overlay_manager.h"

#include "ash/public/cpp/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace arc {

class ArcInputOverlayManagerTest : public testing::Test {
 public:
  ArcInputOverlayManagerTest() = default;

  std::unique_ptr<aura::Window> CreateFakeArcWindow(int window_id) {
    return base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, window_id, gfx::Rect(), nullptr));
  }

  bool IsInputOverlayEnabled(const aura::Window* window) const {
    return arc_test_input_overlay_manager_->input_overlay_enabled_windows_
        .contains(window);
  }

 protected:
  std::unique_ptr<ArcInputOverlayManager> arc_test_input_overlay_manager_;

 private:
  aura::test::TestWindowDelegate dummy_delegate_;

  void SetUp() override {
    arc_test_input_overlay_manager_ =
        base::WrapUnique(new ArcInputOverlayManager(nullptr, nullptr));
  }

  void TearDown() override {
    arc_test_input_overlay_manager_->Shutdown();
    arc_test_input_overlay_manager_.reset();
  }
};

TEST_F(ArcInputOverlayManagerTest, TestPropertyChangeAndWindowDestroy) {
  // Test app with input overlay data.
  auto arc_window = CreateFakeArcWindow(11);
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window.get()));
  arc_window->SetProperty(
      ash::kArcPackageNameKey,
      new std::string("org.chromium.arc.testapp.inputoverlay"));
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window.get()));

  // Test app with input overlay data when window is destroyed.
  arc_window.reset();
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window.get()));

  // Test app without input overlay data.
  auto arc_window_no_data = CreateFakeArcWindow(22);
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_no_data.get()));
  arc_window_no_data->SetProperty(
      ash::kArcPackageNameKey,
      new std::string("org.chromium.arc.testapp.inputoverlay_no_data"));
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_no_data.get()));
}

}  // namespace arc
