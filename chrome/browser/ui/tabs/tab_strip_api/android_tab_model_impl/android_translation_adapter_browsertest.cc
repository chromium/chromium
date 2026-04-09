// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/android_tab_model_impl/android_translation_adapter.h"

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
    injector_ = std::make_unique<tabs_api::AndroidTabStripApiInjector>(model_);
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
  ASSERT_EQ(url, mojo->url);
  ASSERT_EQ(tabs_api::NodeId::FromTabHandle(tab->GetHandle()), mojo->id);
  ASSERT_EQ(base::UTF16ToUTF8(tab->GetContents()->GetTitle()), mojo->title);
  ASSERT_FALSE(mojo->favicon.isNull());
  ASSERT_EQ(tabs::TabNetworkState::kError, mojo->network_state);
  ASSERT_TRUE(mojo->is_active);
  ASSERT_TRUE(mojo->is_selected);
  ASSERT_FALSE(mojo->is_blocked);
}

// TODO(crbug.com/445765534): Add more tests.

}  // namespace tabs_api
