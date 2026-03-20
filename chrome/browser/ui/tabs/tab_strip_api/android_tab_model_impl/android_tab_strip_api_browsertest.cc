// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

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

    auto android_injector =
        std::make_unique<tabs_api::AndroidTabStripApiInjector>(target);
    service_ = std::make_unique<tabs_api::TabStripServiceImpl>(
        std::move(android_injector));
  }

 protected:
  std::unique_ptr<tabs_api::TabStripService> service_;
};

IN_PROC_BROWSER_TEST_F(AndroidTabStripApiBrowserTest, Instantiates) {
  auto result = service_->GetTabs();
  ASSERT_TRUE(result.has_value());
  // Just some hardcoded test.
  ASSERT_EQ(
      "1337",
      result.value()->children.at(0)->children.at(0)->data->get_tab()->id.Id());
}

}  // namespace
