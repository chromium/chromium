// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_translation_adapter.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_tab_strip_api_injector.h"
#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/testing/utils.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tabs_api {

class AndroidTranslationAdapterBrowserTest : public AndroidBrowserTest {
 public:
  AndroidTranslationAdapterBrowserTest() = default;
  ~AndroidTranslationAdapterBrowserTest() override = default;

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    model_ = &testing::GetTabModel(GetProfile());
    injector_ = std::make_unique<tabs_api::AndroidTabStripApiInjector>(
        model_, TabModel::TabLaunchType::FROM_TAB_LIST_INTERFACE);
  }

  void TearDownOnMainThread() override {
    // This needs to be done to ensure that the injector is destroyed before
    // the tab model.
    injector_.reset();

    AndroidBrowserTest::TearDownOnMainThread();
  }

 protected:
  AndroidTranslationAdapter& GetTranslator() {
    return *(static_cast<AndroidTranslationAdapter*>(
        &injector_->translation_adapter()));
  }

  raw_ptr<TabModel> model_;
  std::unique_ptr<AndroidTabStripApiInjector> injector_;
};

IN_PROC_BROWSER_TEST_F(AndroidTranslationAdapterBrowserTest, TranslateTab) {
  GURL url("http://why.hello.there");
  model_->OpenTab(url, 0);

  auto* tab = model_->GetTab(0);
  ASSERT_EQ(url, tab->GetContents()->GetURL());
  WaitForLoadStop(tab->GetContents());

  auto result = GetTranslator().ToMojoTab(tab->GetHandle());
  ASSERT_TRUE(result.has_value());

  auto mojo = std::move(result.value());
  ASSERT_EQ(url, mojo.url);
  ASSERT_EQ(tabs_api::NodeId::FromTabHandle(tab->GetHandle()), mojo.id);
  ASSERT_EQ(tab->GetContents()->GetTitle(), mojo.title);
  ASSERT_FALSE(mojo.favicon.isNull());
  ASSERT_EQ(tabs::TabNetworkState::kError, mojo.network_state);
  ASSERT_TRUE(mojo.is_active);
  ASSERT_TRUE(mojo.is_selected);
  ASSERT_FALSE(mojo.is_blocked);
}

IN_PROC_BROWSER_TEST_F(AndroidTranslationAdapterBrowserTest,
                       TranslateUnloadedTab) {
  GURL bg_url("http://why.hello.there");
  model_->OpenTab(bg_url, 0);
  auto* bg_tab = model_->GetTab(0);
  WaitForLoadStop(bg_tab->GetContents());

  // Open a foreground tab so `bg_tab` is in the background, then release its
  // WebContents to simulate an unloaded background tab on Android.
  GURL fg_url("http://foreground.example");
  model_->OpenTab(fg_url, 1);
  WaitForLoadStop(model_->GetTab(1)->GetContents());

  const tabs::TabHandle bg_handle = bg_tab->GetHandle();
  const GURL expected_url = bg_tab->GetURL();
  const std::u16string expected_title = bg_tab->GetTitle();

  TabAndroid* tab_android = TabAndroid::FromTabHandle(bg_handle);
  ASSERT_NE(nullptr, tab_android);
  std::unique_ptr<content::WebContents> released_contents =
      tab_android->ReleaseWebContentsForTesting();
  ASSERT_EQ(nullptr, bg_tab->GetContents());

  auto result = GetTranslator().ToMojoTab(bg_handle);
  ASSERT_TRUE(result.has_value());

  auto mojo = std::move(result.value());
  EXPECT_EQ(tabs_api::NodeId::FromTabHandle(bg_handle), mojo.id);
  EXPECT_EQ(expected_url, mojo.url);
  EXPECT_EQ(expected_title, mojo.title);
  EXPECT_TRUE(mojo.favicon.isNull());
  EXPECT_EQ(tabs::TabNetworkState::kNone, mojo.network_state);
  EXPECT_FALSE(mojo.is_active);
  EXPECT_FALSE(mojo.last_active.is_null());

  model_->CloseTab(bg_handle);
}

// TODO(crbug.com/445765534): Add more tests.

}  // namespace tabs_api
