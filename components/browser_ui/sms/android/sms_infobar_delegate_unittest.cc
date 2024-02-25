// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/sms/android/sms_infobar_delegate.h"

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/browser_ui/sms/android/sms_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/test_renderer_host.h"

namespace sms {

class SmsInfoBarDelegateTest : public content::RenderViewHostTestHarness {
 public:
  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    infobar_manager_ =
        std::make_unique<infobars::ContentInfoBarManager>(web_contents());
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobar_manager_.get();
  }

 private:
  std::unique_ptr<infobars::ContentInfoBarManager> infobar_manager_;
};

TEST_F(SmsInfoBarDelegateTest, InfoBarForSingleFrame) {
  std::string url = "https://example.com";
  url::Origin origin = url::Origin::Create(GURL(url));
  std::vector<url::Origin> origin_list{origin};
  SmsInfoBar::Create(web_contents(), infobar_manager(), origin_list, "1234",
                     base::OnceClosure(), base::OnceClosure());
  EXPECT_EQ(infobar_manager()->infobars().size(), 1u);
  std::string expected_message = "1234 is your code for example.com";

  EXPECT_EQ(base::UTF16ToUTF8(infobar_manager()
                                  ->infobars()[0]
                                  ->delegate()
                                  ->AsConfirmInfoBarDelegate()
                                  ->GetMessageText()),
            expected_message);
}

TEST_F(SmsInfoBarDelegateTest, InfoBarForEmbeddedFrame) {
  std::string top_url = "https://top.com";
  std::string embedded_url = "https://embedded.com";
  url::Origin top_origin = url::Origin::Create(GURL(top_url));
  url::Origin embedded_origin = url::Origin::Create(GURL(embedded_url));
  std::vector<url::Origin> origin_list{embedded_origin, top_origin};
  SmsInfoBar::Create(web_contents(), infobar_manager(), origin_list, "1234",
                     base::OnceClosure(), base::OnceClosure());
  EXPECT_EQ(infobar_manager()->infobars().size(), 1u);
  std::string expected_message =
      "1234 is your code for embedded.com to continue on top.com";
  EXPECT_EQ(base::UTF16ToUTF8(infobar_manager()
                                  ->infobars()[0]
                                  ->delegate()
                                  ->AsConfirmInfoBarDelegate()
                                  ->GetMessageText()),
            expected_message);
}

}  // namespace sms
