// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class AlternateNavInfoBarDelegateTest : public ChromeRenderViewHostTestHarness {
 protected:
  AlternateNavInfoBarDelegateTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    layout_provider_ = std::make_unique<ChromeLayoutProvider>();
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
  }

  infobars::InfoBarDelegate* CreateDelegate(
      const GURL& destination_url = GURL("https://youtube.com/"),
      const GURL& search_url = GURL("https://google.com/")) {
    AutocompleteMatch match;
    match.destination_url = destination_url;

    AlternateNavInfoBarDelegate::CreateForOmniboxNavigation(
        web_contents(), u"test", match, search_url);

    auto* manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents());
    if (manager->infobars().empty()) {
      return nullptr;
    }

    return manager->infobars()[0]->delegate();
  }

 private:
  std::unique_ptr<ChromeLayoutProvider> layout_provider_;
};

TEST_F(AlternateNavInfoBarDelegateTest, BasicProperties) {
  auto* delegate = CreateDelegate();
  ASSERT_TRUE(delegate);

  EXPECT_EQ(infobars::InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE,
            delegate->GetIdentifier());
  EXPECT_EQ(GURL("https://youtube.com/"), delegate->GetLinkURL());
}

TEST_F(AlternateNavInfoBarDelegateTest, LinkTextFlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInfoBarInlineLinks);

  auto* delegate = CreateDelegate();
  ASSERT_TRUE(delegate);

  EXPECT_EQ(u"https://youtube.com/", delegate->GetLinkText());
}

TEST_F(AlternateNavInfoBarDelegateTest, LinkTextFlagOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInfoBarInlineLinks);

  auto* delegate = CreateDelegate();
  ASSERT_TRUE(delegate);

  EXPECT_EQ(u"", delegate->GetLinkText());
}

TEST_F(AlternateNavInfoBarDelegateTest, ConfirmInfoBarDelegateOverrides) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kInfoBarInlineLinks);

  auto* delegate = CreateDelegate();
  ASSERT_TRUE(delegate);

  auto* confirm_delegate = delegate->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm_delegate);

  EXPECT_EQ(u"Did you mean to go to https://youtube.com/?",
            confirm_delegate->GetMessageText());
  EXPECT_EQ(u"Did you mean to go to $1?",
            confirm_delegate->GetMessageTextTemplate());

  auto substitutions = confirm_delegate->GetMessageSubstitutions();
  ASSERT_EQ(1u, substitutions.size());
  EXPECT_EQ(u"https://youtube.com/", substitutions[0].text);
  EXPECT_TRUE(substitutions[0].is_link);
}
