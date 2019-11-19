// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_INFOBAR_H_
#define CHROME_BROWSER_UI_TEST_TEST_INFOBAR_H_

#include "base/optional.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"

class InfoBarService;

namespace content {
class WebContents;
}  // namespace content

class TestInfoBar : public UiBrowserTest {
 public:
  TestInfoBar();
  TestInfoBar(const TestInfoBar&) = delete;
  TestInfoBar& operator=(const TestInfoBar&) = delete;
  ~TestInfoBar() override;

  // UiBrowserTest:
  void PreShow() override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 protected:
  using InfoBarDelegateIdentifier =
      infobars::InfoBarDelegate::InfoBarIdentifier;
  void AddExpectedInfoBar(InfoBarDelegateIdentifier identifier);

  // Returns the active tab.
  content::WebContents* GetWebContents();
  const content::WebContents* GetWebContents() const;

  // Returns the InfoBarService associated with the active tab.
  InfoBarService* GetInfoBarService();
  const InfoBarService* GetInfoBarService() const;

 private:
  using InfoBars = infobars::InfoBarManager::InfoBars;

  // Returns the current infobars that are not already in |starting_infobars_|.
  // Fails (i.e. returns nullopt) if the current set of infobars does not begin
  // with |starting_infobars_|.
  base::Optional<InfoBars> GetNewInfoBars() const;

  InfoBars starting_infobars_;
  std::vector<InfoBarDelegateIdentifier> expected_identifiers_;
};

#endif  // CHROME_BROWSER_UI_TEST_TEST_INFOBAR_H_
