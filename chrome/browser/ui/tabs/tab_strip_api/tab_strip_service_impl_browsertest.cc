// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

class MockTabsObserver : public tabs_api::mojom::TabsObserver {
 public:
  MockTabsObserver() = default;
  ~MockTabsObserver() override = default;

  MOCK_METHOD(void,
              OnTabsCreated,
              (std::vector<tabs_api::mojom::PositionPtr> positions),
              (override));
};

class TabStripServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  TabStripServiceImplBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kTabStripBrowserApi);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    tab_strip_service_impl_ = std::make_unique<TabStripServiceImpl>(
        browser(), browser()->tab_strip_model());
  }

  void TearDownOnMainThread() override {
    tab_strip_service_impl_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void CreateTabAtApiCallback(
      base::RunLoop* run_loop,
      base::expected<bool, mojo_base::mojom::ErrorPtr>* out_result,
      base::expected<bool, mojo_base::mojom::ErrorPtr> result) {
    *out_result = std::move(result);
    if (run_loop) {
      run_loop->Quit();
    }
  }

 protected:
  TabStripModel* GetTabStripModel() { return browser()->tab_strip_model(); }

  tabs_api::mojom::PositionPtr CreatePosition(int index) {
    auto position = tabs_api::mojom::Position::New();
    position->index = index;
    return position;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TabStripServiceImpl> tab_strip_service_impl_;
};

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, CreateTabAt) {
  TabStripModel* model = GetTabStripModel();
  const int expected_tab_count = model->count() + 1;
  const GURL url("http://example.com/");

  base::expected<bool, mojo_base::mojom::ErrorPtr> result;
  base::RunLoop run_loop;
  tabs_api::mojom::PositionPtr position = CreatePosition(0);

  tab_strip_service_impl_->CreateTabAt(
      std::move(position), std::make_optional(url),
      base::BindOnce(&TabStripServiceImplBrowserTest::CreateTabAtApiCallback,
                     base::Unretained(this), &run_loop, &result));
  run_loop.Run();

  ASSERT_TRUE(result.has_value())
      << "CreateTabAt failed: " << (result.error()->message);
  EXPECT_TRUE(result.value());
  EXPECT_EQ(model->count(), expected_tab_count);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceImplBrowserTest, ObserverOnTabsCreated) {
  MockTabsObserver mock_observer;
  tab_strip_service_impl_->AddObserver(&mock_observer);
  const GURL url("http://example.com/");
  uint32_t target_index = 0;

  EXPECT_CALL(
      mock_observer,
      OnTabsCreated(testing::Truly(
          [&target_index](
              const std::vector<tabs_api::mojom::PositionPtr>& positions) {
            if (positions.size() != 1) {
              return false;
            }
            if (!positions[0]) {
              return false;
            }
            return positions[0]->index == target_index;
          })))
      .Times(1);

  base::expected<bool, mojo_base::mojom::ErrorPtr> result;
  base::RunLoop run_loop;
  tabs_api::mojom::PositionPtr position = CreatePosition(target_index);

  tab_strip_service_impl_->CreateTabAt(
      std::move(position), std::make_optional(url),
      base::BindOnce(&TabStripServiceImplBrowserTest::CreateTabAtApiCallback,
                     base::Unretained(this), &run_loop, &result));
  run_loop.Run();

  ASSERT_TRUE(result.has_value())
      << "CreateTabAt failed: " << (result.error()->message);
  EXPECT_TRUE(result.value());

  tab_strip_service_impl_->RemoveObserver(&mock_observer);
}
