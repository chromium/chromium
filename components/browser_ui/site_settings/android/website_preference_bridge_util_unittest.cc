// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/site_settings/android/website_preference_bridge_util.h"

#include "content/public/test/test_renderer_host.h"

class ClearLocalStorageHelperTest : public content::RenderViewHostTestHarness {
 public:
  void OnLocalStorageCleared() {
    callback_invoked_ = true;
    loop_.Quit();
    auto* remover = browser_context()->GetBrowsingDataRemover();
    EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE,
              remover->GetLastUsedRemovalMaskForTesting());
    EXPECT_EQ(base::Time::Min(), remover->GetLastUsedBeginTimeForTesting());
    EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
              remover->GetLastUsedOriginTypeMaskForTesting());
  }

  bool callback_invoked_ = false;
  base::RunLoop loop_;
};

TEST_F(ClearLocalStorageHelperTest, TestDeletion) {
  ClearLocalStorageHelper::ClearLocalStorage(
      browser_context(), url::Origin::Create(GURL("https://example.com")),
      base::BindOnce(&ClearLocalStorageHelperTest::OnLocalStorageCleared,
                     base::Unretained(this)));
  loop_.Run();
  EXPECT_TRUE(callback_invoked_);
}
