// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class AndroidTabStripApiBrowserTest : public AndroidBrowserTest {
 public:
  AndroidTabStripApiBrowserTest() = default;
  ~AndroidTabStripApiBrowserTest() override = default;

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    TabModel* target;
    for (TabModel* model : TabModelList::models()) {
      if (model->GetProfile() == GetProfile()) {
        target = model;
        break;
      }
    }
    CHECK(target) << "could not find a tab model to construct the api with";

    model_ = target;
    auto android_injector =
        std::make_unique<tabs_api::AndroidTabStripApiInjector>(target);
    service_ = std::make_unique<tabs_api::TabStripServiceImpl>(
        std::move(android_injector));
  }

 protected:
  raw_ptr<TabModel> model_;
  std::unique_ptr<tabs_api::TabStripService> service_;
};

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Instantiates) {
  // Initial state test, there should be one tab.
  ASSERT_EQ(1, model_->GetTabCount());
  {
    auto result = service_->GetTabs();
    ASSERT_TRUE(result.has_value());

    auto& window_container = result.value();
    ASSERT_EQ(base::NumberToString(model_->GetSessionId().id()),
              window_container->data->get_window()->id.Id());
    ASSERT_EQ(1u, window_container->children.size());

    auto& tab_strip_container = window_container->children.at(0);
    ASSERT_EQ("-", tab_strip_container->data->get_tab_strip()->id.Id());
    ASSERT_EQ(1u, tab_strip_container->children.size());

    ASSERT_EQ(base::NumberToString(
                  model_->GetAllTabs().at(0)->GetHandle().raw_value()),
              tab_strip_container->children.at(0)->data->get_tab()->id.Id());
  }

  // Now create a new tab and check that it is indeed reflected.
  model_->CreateNewTabForDevTools(GURL("http://somewhere.nowhere"), false);
  ASSERT_EQ(2, model_->GetTabCount());
  {
    // Some of the stuff is repeated, just to make sure we don't mangle the
    // parents.
    auto result = service_->GetTabs();
    ASSERT_TRUE(result.has_value());

    auto& window_container = result.value();
    ASSERT_EQ(base::NumberToString(model_->GetSessionId().id()),
              window_container->data->get_window()->id.Id());
    ASSERT_EQ(1u, window_container->children.size());

    auto& tab_strip_container = window_container->children.at(0);
    ASSERT_EQ("-", tab_strip_container->data->get_tab_strip()->id.Id());
    ASSERT_EQ(2u, tab_strip_container->children.size());

    // Ordering is actually material, we need to ensure that the tab
    // order returned by the API matches the underlying model.
    ASSERT_EQ(base::NumberToString(model_->GetTab(0)->GetHandle().raw_value()),
              tab_strip_container->children.at(0)->data->get_tab()->id.Id());
    ASSERT_EQ(base::NumberToString(model_->GetTab(1)->GetHandle().raw_value()),
              tab_strip_container->children.at(1)->data->get_tab()->id.Id());
  }
}

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Create) {
  ASSERT_EQ(1, model_->GetTabCount());

  auto result = service_->CreateTabAt(std::nullopt, GURL("http://there.where"));

  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(2, model_->GetTabCount());
}

}  // namespace
