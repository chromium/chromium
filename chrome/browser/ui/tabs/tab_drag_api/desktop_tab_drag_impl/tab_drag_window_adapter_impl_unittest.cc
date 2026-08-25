// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_window_adapter_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_window/test/fake_global_browser_collection.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/mock_base_window.h"
#include "ui/views/widget/widget.h"

using tabs_api::NodeId;
using tabs_api::TabDragWindowId;
using testing::Return;

class TestGlobalFeatures : public GlobalFeatures {
 public:
  TestGlobalFeatures() = default;
  ~TestGlobalFeatures() override = default;

 protected:
  std::unique_ptr<GlobalBrowserCollection> CreateGlobalBrowserCollection()
      override {
    return std::make_unique<FakeGlobalBrowserCollection>();
  }
};

class TabDragWindowAdapterImplTest : public ChromeViewsTestBase {
 public:
  TabDragWindowAdapterImplTest() = default;
  ~TabDragWindowAdapterImplTest() override = default;

  void SetUp() override {
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(
        base::BindRepeating(&TabDragWindowAdapterImplTest::CreateGlobalFeatures,
                            base::Unretained(this)));

    // Initialize GlobalFeatures on the TestingBrowserProcess instance created
    // by the unit_tests runner.
    testing_profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

    ChromeViewsTestBase::SetUp();

    fake_collection_ = static_cast<FakeGlobalBrowserCollection*>(
        TestingBrowserProcess::GetGlobal()
            ->GetFeatures()
            ->global_browser_collection());
    ASSERT_TRUE(fake_collection_);

    profile_ = testing_profile_manager_->CreateTestingProfile("default");
    model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
  }

  void TearDown() override {
    model_delegate_.reset();
    profile_ = nullptr;
    testing_profile_manager_ = nullptr;
    fake_collection_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    ChromeViewsTestBase::TearDown();
    GlobalFeatures::ReplaceGlobalFeaturesForTesting(base::NullCallback());
  }

  std::unique_ptr<GlobalFeatures> CreateGlobalFeatures() {
    return std::make_unique<TestGlobalFeatures>();
  }

 protected:
  raw_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<FakeGlobalBrowserCollection> fake_collection_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestTabStripModelDelegate> model_delegate_;
  tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_feature_initialization_;
};

TEST_F(TabDragWindowAdapterImplTest, ShouldDragWholeWindow) {
  testing::NiceMock<MockBrowserWindowInterface> browser_window;
  testing::NiceMock<ui::MockBaseWindow> base_window;
  TabStripModel model(model_delegate_.get(), profile_);

  EXPECT_CALL(browser_window, GetTabStripModel())
      .WillRepeatedly(Return(&model));
  EXPECT_CALL(browser_window, GetWindow()).WillRepeatedly(Return(&base_window));
  EXPECT_CALL(base_window, IsFullscreen()).WillRepeatedly(Return(false));

  TabDragWindowAdapterImpl adapter(&browser_window);

  // Case 1: Empty model, empty indices.
  EXPECT_TRUE(adapter.ShouldDragWholeWindow(0));

  // Case 2: Fullscreen window should not drag whole window.
  EXPECT_CALL(base_window, IsFullscreen())
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_FALSE(adapter.ShouldDragWholeWindow(0));

  // Case 3: Model has tabs, indices match all tabs.
  std::unique_ptr<content::WebContents> wc1 = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  model.AppendWebContents(std::move(wc1), true);
  EXPECT_EQ(model.count(), 1);
  EXPECT_TRUE(adapter.ShouldDragWholeWindow(1));
  EXPECT_FALSE(adapter.ShouldDragWholeWindow(0));

  std::unique_ptr<content::WebContents> wc2 = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  model.AppendWebContents(std::move(wc2), true);
  EXPECT_EQ(model.count(), 2);
  EXPECT_TRUE(adapter.ShouldDragWholeWindow(2));
  EXPECT_FALSE(adapter.ShouldDragWholeWindow(1));
}

TEST_F(TabDragWindowAdapterImplTest, MigrateTabs) {
  testing::NiceMock<MockBrowserWindowInterface> source_window;
  testing::NiceMock<MockBrowserWindowInterface> target_window;
  testing::NiceMock<ui::MockBaseWindow> source_base_window;
  testing::NiceMock<ui::MockBaseWindow> target_base_window;

  TabStripModel source_model(model_delegate_.get(), profile_);
  TabStripModel target_model(model_delegate_.get(), profile_);

  std::unique_ptr<views::Widget> source_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  std::unique_ptr<views::Widget> target_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  gfx::NativeWindow source_native_window = source_widget->GetNativeWindow();
  gfx::NativeWindow target_native_window = target_widget->GetNativeWindow();

  EXPECT_CALL(source_base_window, GetNativeWindow())
      .WillRepeatedly(Return(source_native_window));
  EXPECT_CALL(target_base_window, GetNativeWindow())
      .WillRepeatedly(Return(target_native_window));

  EXPECT_CALL(source_window, GetTabStripModel())
      .WillRepeatedly(Return(&source_model));
  EXPECT_CALL(target_window, GetTabStripModel())
      .WillRepeatedly(Return(&target_model));
  EXPECT_CALL(source_window, GetWindow())
      .WillRepeatedly(Return(&source_base_window));
  EXPECT_CALL(target_window, GetWindow())
      .WillRepeatedly(Return(&target_base_window));
  EXPECT_CALL(target_window, GetProfile()).WillRepeatedly(Return(profile_));

  fake_collection_->SimulateBrowserCreated(&target_window);

  TabDragWindowAdapterImpl source_adapter(&source_window);
  TabDragWindowAdapterImpl target_adapter(&target_window);

  auto* registry = TestingBrowserProcess::GetGlobal()
                       ->GetFeatures()
                       ->tab_drag_session_manager()
                       ->GetWindowRegistry();
  ASSERT_TRUE(registry);

  TabDragWindowId source_id =
      registry->FindWindowIdByNativeWindow(source_native_window);
  TabDragWindowId target_id =
      registry->FindWindowIdByNativeWindow(target_native_window);
  ASSERT_FALSE(source_id.is_null());
  ASSERT_FALSE(target_id.is_null());

  // Add one tab to source model.
  std::unique_ptr<content::WebContents> wc = content::WebContents::Create(
      content::WebContents::CreateParams(profile_));
  content::WebContents* wc_ptr = wc.get();
  source_model.AppendWebContents(std::move(wc), true);

  ASSERT_EQ(source_model.count(), 1);
  ASSERT_EQ(target_model.count(), 0);

  // Migrate the tab from source to target window.
  tabs::TabHandle handle = source_model.GetTabAtIndex(0)->GetHandle();
  auto result =
      source_adapter.MigrateTabs(target_id, {NodeId::FromTabHandle(handle)});
  EXPECT_TRUE(result.has_value());

  EXPECT_EQ(source_model.count(), 0);
  EXPECT_EQ(target_model.count(), 1);
  EXPECT_EQ(target_model.GetWebContentsAt(0), wc_ptr);

  fake_collection_->SimulateBrowserClosed(&target_window);
}
