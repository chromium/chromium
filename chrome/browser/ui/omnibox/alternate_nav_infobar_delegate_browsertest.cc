// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"
#include "chrome/browser/ui/test/test_infobar.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {

class AlternateNavInfoBarDelegateTest : public TestInfoBar {
 public:
  AlternateNavInfoBarDelegateTest(const AlternateNavInfoBarDelegateTest&) =
      delete;
  AlternateNavInfoBarDelegateTest& operator=(
      const AlternateNavInfoBarDelegateTest&) = delete;

 protected:
  AlternateNavInfoBarDelegateTest() = default;
  ~AlternateNavInfoBarDelegateTest() override = default;

 private:
  // TestInfoBar:
  void ShowUi(const std::string& name) override {
    AddExpectedInfoBar(infobars::InfoBarDelegate::InfoBarIdentifier::
                           ALTERNATE_NAV_INFOBAR_DELEGATE);
    AutocompleteMatch match;
    match.destination_url = GURL("http://intranetsite/");
    ChromeOmniboxNavigationObserver::ShowAlternativeNavInfoBar(
        GetWebContents(), std::u16string(), match, GURL("http://example.com/"));
  }
};

class AlternateNavInfoBarDelegateInlineLinksTest
    : public AlternateNavInfoBarDelegateTest {
 protected:
  AlternateNavInfoBarDelegateInlineLinksTest() {
    feature_list_.InitAndEnableFeature(features::kInfoBarInlineLinks);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AlternateNavInfoBarDelegateCentralizedTest
    : public AlternateNavInfoBarDelegateTest {
 protected:
  AlternateNavInfoBarDelegateCentralizedTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        infobars::kCentralizedInfoBarFramework,
        {{"MigratedAlternateNav", "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AlternateNavInfoBarDelegateCentralizedInlineLinksTest
    : public AlternateNavInfoBarDelegateTest {
 protected:
  AlternateNavInfoBarDelegateCentralizedInlineLinksTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{infobars::kCentralizedInfoBarFramework,
          {{"MigratedAlternateNav", "true"}}},
         {features::kInfoBarInlineLinks, {}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AlternateNavInfoBarDelegateTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AlternateNavInfoBarDelegateInlineLinksTest,
                       InvokeUi_InlineLinks) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AlternateNavInfoBarDelegateCentralizedTest,
                       InvokeUi_Centralized) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AlternateNavInfoBarDelegateCentralizedInlineLinksTest,
                       InvokeUi_CentralizedInlineLinks) {
  ShowAndVerifyUi();
}
