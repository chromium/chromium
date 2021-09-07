// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/arc_input_overlay_manager.h"

#include "ash/public/cpp/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_factory.h"

namespace arc {

class ArcInputOverlayManagerTest : public aura::test::AuraTestBase {
 public:
  ArcInputOverlayManagerTest() = default;

  std::unique_ptr<aura::Window> CreateFakeArcWindow(int window_id,
                                                    aura::Window* root) {
    return base::WrapUnique(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, window_id, gfx::Rect(), root));
  }

  bool IsInputOverlayEnabled(const aura::Window* window) const {
    return arc_test_input_overlay_manager_->input_overlay_enabled_windows_
        .contains(window);
  }

  ui::InputMethod* GetInputMethod() {
    return arc_test_input_overlay_manager_->input_method_;
  }

  bool IsTextInputActive() {
    return arc_test_input_overlay_manager_->is_text_input_active_;
  }

 protected:
  std::unique_ptr<ArcInputOverlayManager> arc_test_input_overlay_manager_;

 private:
  aura::test::TestWindowDelegate dummy_delegate_;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();
    arc_test_input_overlay_manager_ =
        base::WrapUnique(new ArcInputOverlayManager(nullptr, nullptr));
  }

  void TearDown() override {
    arc_test_input_overlay_manager_->Shutdown();
    arc_test_input_overlay_manager_.reset();
    aura::test::AuraTestBase::TearDown();
  }
};

TEST_F(ArcInputOverlayManagerTest, TestPropertyChangeAndWindowDestroy) {
  // Test app with input overlay data.
  auto arc_window = CreateFakeArcWindow(11, nullptr);
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window.get()));
  arc_window->SetProperty(
      ash::kArcPackageNameKey,
      new std::string("org.chromium.arc.testapp.inputoverlay"));
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window.get()));

  // Test app with input overlay data when window is destroyed.
  arc_window.reset();
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window.get()));

  // Test app without input overlay data.
  auto arc_window_no_data = CreateFakeArcWindow(22, nullptr);
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_no_data.get()));
  arc_window_no_data->SetProperty(
      ash::kArcPackageNameKey,
      new std::string("org.chromium.arc.testapp.inputoverlay_no_data"));
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_no_data.get()));
}

TEST_F(ArcInputOverlayManagerTest, TestInputMethodObsever) {
  ASSERT_FALSE(GetInputMethod());
  ASSERT_FALSE(IsTextInputActive());
  auto arc_window = CreateFakeArcWindow(11, root_window());
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window.get()));
  arc_window->SetProperty(
      ash::kArcPackageNameKey,
      new std::string("org.chromium.arc.testapp.inputoverlay"));
  ui::InputMethod* input_method = GetInputMethod();
  EXPECT_TRUE(GetInputMethod());
  input_method->SetFocusedTextInputClient(nullptr);
  EXPECT_FALSE(IsTextInputActive());
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&dummy_text_input_client);
  EXPECT_TRUE(IsTextInputActive());
  ui::DummyTextInputClient dummy_text_none_input_client(
      ui::TEXT_INPUT_TYPE_NONE);
  input_method->SetFocusedTextInputClient(&dummy_text_none_input_client);
  EXPECT_FALSE(IsTextInputActive());
  input_method->SetFocusedTextInputClient(nullptr);
}

}  // namespace arc
