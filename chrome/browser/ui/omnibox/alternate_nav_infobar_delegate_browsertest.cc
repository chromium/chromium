// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"

#include "chrome/browser/ui/test/test_infobar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace {

class AlternateNavInfoBarDelegateTest : public TestInfoBar {
 public:
  AlternateNavInfoBarDelegateTest() = default;
  AlternateNavInfoBarDelegateTest(const AlternateNavInfoBarDelegateTest&) =
      delete;
  AlternateNavInfoBarDelegateTest& operator=(
      const AlternateNavInfoBarDelegateTest&) = delete;
  ~AlternateNavInfoBarDelegateTest() override = default;

 private:
  // TestInfoBar:
  void ShowUi(const std::string& name) override {
    AddExpectedInfoBar(infobars::InfoBarDelegate::InfoBarIdentifier::
                           ALTERNATE_NAV_INFOBAR_DELEGATE);
    AutocompleteMatch match;
    match.destination_url = GURL("http://intranetsite/");
    AlternateNavInfoBarDelegate::CreateForOmniboxNavigation(
        GetWebContents(), std::u16string(), match, GURL("http://example.com/"));
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AlternateNavInfoBarDelegateTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
