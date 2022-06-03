// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/rounded_window_corners_manager.h"

#include "chromecast/graphics/cast_window_manager_aura.h"
#include "components/exo/surface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer_type.h"

using ::testing::StrictMock;

namespace chromecast {

class MockCastWindowManager : public CastWindowManagerAura {
 public:
  explicit MockCastWindowManager(bool enable_input)
      : CastWindowManagerAura(enable_input),
        root_window_(new aura::Window(nullptr)) {
    root_window_->Init(ui::LAYER_TEXTURED);
  }

  MOCK_METHOD(void, SetEnableRoundedCorners, (bool enable), (override));

  aura::Window* GetRootWindow() override { return root_window_.get(); }

 private:
  std::unique_ptr<aura::Window> root_window_;
};

class RoundedWindowCornersManagerTest : public testing::Test {
 public:
  RoundedWindowCornersManagerTest()
      : mock_cast_window_manager_(new StrictMock<MockCastWindowManager>(true)),
        rounded_window_corners_manager_(
            new RoundedWindowCornersManager(mock_cast_window_manager_.get())) {}

  RoundedWindowCornersManagerTest(const RoundedWindowCornersManagerTest&) =
      delete;
  RoundedWindowCornersManagerTest& operator=(
      const RoundedWindowCornersManagerTest&) = delete;

 protected:
  std::unique_ptr<StrictMock<MockCastWindowManager>> mock_cast_window_manager_;
  std::unique_ptr<RoundedWindowCornersManager> rounded_window_corners_manager_;
};

TEST_F(RoundedWindowCornersManagerTest, NoSetProperty) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  root_window->AddChild(window.get());
  window->Show();
  window->Hide();
}

TEST_F(RoundedWindowCornersManagerTest,
       SetRoundedCornersOnWindowAfterSettingExoPropertyAndShowing) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->SetProperty(exo::kClientSurfaceIdKey, std::string("1"));
  root_window->AddChild(window.get());
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest,
       SetRoundedCornersOnVisibleWindowAfterSettingExoProperty) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->Show();
  root_window->AddChild(window.get());
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window->SetProperty(exo::kClientSurfaceIdKey, std::string("1"));

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest, RemoveRoundedCornersAfterHidingWindow) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->SetProperty(exo::kClientSurfaceIdKey, std::string("1"));
  root_window->AddChild(window.get());
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window->Hide();
}

TEST_F(RoundedWindowCornersManagerTest,
       RemoveRoundedCornersAfterHidingMultipleWindows) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window1 =
      std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_TEXTURED);
  std::unique_ptr<aura::Window> window2 =
      std::make_unique<aura::Window>(nullptr);
  window2->Init(ui::LAYER_TEXTURED);
  window1->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  window2->SetProperty(exo::kClientSurfaceIdKey, new std::string("2"));
  root_window->AddChild(window1.get());
  root_window->AddChild(window2.get());
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window1->Show();
  window2->Show();
  window1->Hide();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window2->Hide();
}

TEST_F(RoundedWindowCornersManagerTest,
       RemoveRoundedCornersAfterDestroyingMultipleWindows) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window1 =
      std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_TEXTURED);
  std::unique_ptr<aura::Window> window2 =
      std::make_unique<aura::Window>(nullptr);
  window2->Init(ui::LAYER_TEXTURED);
  window1->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  window2->SetProperty(exo::kClientSurfaceIdKey, new std::string("2"));
  root_window->AddChild(window1.get());
  root_window->AddChild(window2.get());
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  window1->Show();
  window2->Show();
  window1 = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  window2 = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest,
       DoNotAddRoundedCornersWhenSiblingShownFirst) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> webview =
      std::make_unique<aura::Window>(nullptr);
  webview->Init(ui::LAYER_TEXTURED);
  webview->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  std::unique_ptr<aura::Window> sibling =
      std::make_unique<aura::Window>(nullptr);
  sibling->Init(ui::LAYER_TEXTURED);
  root_window->AddChild(webview.get());
  root_window->AddChild(sibling.get());
  sibling->Show();
  webview->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  sibling = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  webview = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest,
       RemoveRoundedCornersWhenSiblingShownSecond) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> webview =
      std::make_unique<aura::Window>(nullptr);
  webview->Init(ui::LAYER_TEXTURED);
  webview->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  std::unique_ptr<aura::Window> sibling =
      std::make_unique<aura::Window>(nullptr);
  sibling->Init(ui::LAYER_TEXTURED);
  root_window->AddChild(webview.get());
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  webview->Show();
  root_window->AddChild(sibling.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  sibling->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  sibling = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  webview = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest,
       RemoveRoundedCornersWhenSiblingAddedToHierarchyFirstAfterShown) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> webview =
      std::make_unique<aura::Window>(nullptr);
  webview->Init(ui::LAYER_TEXTURED);
  webview->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  std::unique_ptr<aura::Window> sibling =
      std::make_unique<aura::Window>(nullptr);
  sibling->Init(ui::LAYER_TEXTURED);
  sibling->Show();
  root_window->AddChild(sibling.get());
  root_window->AddChild(webview.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  webview->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
}

TEST_F(RoundedWindowCornersManagerTest,
       RemoveRoundedCornersWhenSiblingAddedToHierarchySecondAfterShown) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> webview =
      std::make_unique<aura::Window>(nullptr);
  webview->Init(ui::LAYER_TEXTURED);
  webview->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  std::unique_ptr<aura::Window> sibling =
      std::make_unique<aura::Window>(nullptr);
  sibling->Init(ui::LAYER_TEXTURED);
  sibling->Show();
  root_window->AddChild(webview.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  webview->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  root_window->AddChild(sibling.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  sibling = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  webview = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest, AddRoundedCornersWhenSiblingIsHidden) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> webview =
      std::make_unique<aura::Window>(nullptr);
  webview->Init(ui::LAYER_TEXTURED);
  webview->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  std::unique_ptr<aura::Window> sibling =
      std::make_unique<aura::Window>(nullptr);
  sibling->Init(ui::LAYER_TEXTURED);
  root_window->AddChild(webview.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  webview->Show();
  root_window->AddChild(sibling.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  sibling->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  sibling->Hide();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  sibling = nullptr;
  webview = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest,
       AddRoundedCornersWhenSiblingUnderWebview) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> webview =
      std::make_unique<aura::Window>(nullptr);
  webview->Init(ui::LAYER_TEXTURED);
  webview->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  std::unique_ptr<aura::Window> sibling =
      std::make_unique<aura::Window>(nullptr);
  sibling->Init(ui::LAYER_TEXTURED);
  root_window->AddChild(sibling.get());
  sibling->Show();
  root_window->AddChild(webview.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  webview->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  webview = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest,
       AddRoundedCornersWhenTopWindowNotSiblingOfWebview) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window_host =
      std::make_unique<aura::Window>(nullptr);
  window_host->Init(ui::LAYER_TEXTURED);
  window_host->Show();
  root_window->AddChild(window_host.get());

  std::unique_ptr<aura::Window> webview =
      std::make_unique<aura::Window>(nullptr);
  webview->Init(ui::LAYER_TEXTURED);
  webview->SetProperty(exo::kClientSurfaceIdKey, new std::string("1"));
  std::unique_ptr<aura::Window> sibling =
      std::make_unique<aura::Window>(nullptr);
  sibling->Init(ui::LAYER_TEXTURED);
  window_host->AddChild(webview.get());
  window_host->AddChild(sibling.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  webview->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  sibling->Show();

  std::unique_ptr<aura::Window> volume_window =
      std::make_unique<aura::Window>(nullptr);
  volume_window->Init(ui::LAYER_TEXTURED);
  volume_window->SetId(CastWindowManager::VOLUME);
  root_window->AddChild(volume_window.get());
  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  volume_window->Show();

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  volume_window = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  sibling = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  webview = nullptr;
}

TEST_F(RoundedWindowCornersManagerTest, UnmanagedAppWithChild) {
  aura::Window* root_window = mock_cast_window_manager_->GetRootWindow();
  root_window->Show();
  std::unique_ptr<aura::Window> window_host =
      std::make_unique<aura::Window>(nullptr);
  window_host->Init(ui::LAYER_TEXTURED);
  window_host->Show();
  root_window->AddChild(window_host.get());
  std::unique_ptr<aura::Window> unmanaged_app =
      std::make_unique<aura::Window>(nullptr);
  unmanaged_app->Init(ui::LAYER_TEXTURED);
  unmanaged_app->SetId(CastWindowManager::UNMANAGED_APP);
  window_host->AddChild(unmanaged_app.get());

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(true));
  unmanaged_app->Show();

  std::unique_ptr<aura::Window> child = std::make_unique<aura::Window>(nullptr);
  child->Init(ui::LAYER_TEXTURED);
  unmanaged_app->AddChild(child.get());
  // Rounded corners should be retained if the unmanaged app parent is visible.
  child->Show();
  child = nullptr;

  EXPECT_CALL(*mock_cast_window_manager_, SetEnableRoundedCorners(false));
  unmanaged_app = nullptr;
}

}  // namespace chromecast
