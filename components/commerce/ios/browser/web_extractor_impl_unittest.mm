// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/commerce/ios/browser/web_extractor_impl.h"

#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/ios/browser/web_extractor_impl.h"
#import "components/commerce/ios/browser/web_state_wrapper.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

class WebExtractorImplTest : public web::WebTestWithWebState {
 public:
  WebExtractorImplTest() = default;

  WebExtractorImplTest(const WebExtractorImplTest&) = delete;
  WebExtractorImplTest& operator=(const WebExtractorImplTest&) = delete;

 private:
  // This is required to make sure that all DataDecoders constructed during its
  // lifetime will connect to this instance rather than launching a separate
  // process.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

#if BUILDFLAG(USE_BLINK)
// TODO(crbug.com/40288232): breaks a threading assumption with use_blink.
#define MAYBE_TestValidMetaExtraction DISABLED_TestValidMetaExtraction
#else
#define MAYBE_TestValidMetaExtraction TestValidMetaExtraction
#endif  // BUILDFLAG(USE_BLINK)
TEST_F(WebExtractorImplTest, MAYBE_TestValidMetaExtraction) {
  ASSERT_TRUE(LoadHtml("<html>"
                       "<head>"
                       "<meta content=\"product\" property=\"og:type\">"
                       "</head>"
                       "<body>"
                       "</body></html>"));

  std::unique_ptr<commerce::WebStateWrapper> web_wrapper =
      std::make_unique<commerce::WebStateWrapper>(web_state());
  std::unique_ptr<commerce::WebExtractorImpl> web_extractor =
      std::make_unique<commerce::WebExtractorImpl>();
  __block bool callback_called = false;

  web_extractor->ExtractMetaInfo(
      web_wrapper.get(), base::BindOnce(^(const base::Value result) {
        ASSERT_TRUE(result.is_dict());
        auto* str = result.GetDict().FindString(commerce::kOgType);
        ASSERT_TRUE(str);
        ASSERT_EQ(*str, commerce::kOgTypeOgProduct);
        callback_called = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
}

#if BUILDFLAG(USE_BLINK)
// TODO(crbug.com/40288232): breaks a threading assumption with use_blink.
#define MAYBE_TestInvalidMetaExtraction DISABLED_TestInvalidMetaExtraction
#else
#define MAYBE_TestInvalidMetaExtraction TestInvalidMetaExtraction
#endif  // BUILDFLAG(USE_BLINK)
TEST_F(WebExtractorImplTest, MAYBE_TestInvalidMetaExtraction) {
  ASSERT_TRUE(LoadHtml("<html>"
                       "<head>"
                       "<meta content=\"product\" property=\"type\">"
                       "<meta content=\"product\" type=\"og:type\">"
                       "</head>"
                       "<body>"
                       "</body></html>"));

  std::unique_ptr<commerce::WebStateWrapper> web_wrapper =
      std::make_unique<commerce::WebStateWrapper>(web_state());
  std::unique_ptr<commerce::WebExtractorImpl> web_extractor =
      std::make_unique<commerce::WebExtractorImpl>();
  __block bool callback_called = false;

  web_extractor->ExtractMetaInfo(web_wrapper.get(),
                                 base::BindOnce(^(const base::Value result) {
                                   ASSERT_TRUE(result.is_dict());
                                   ASSERT_TRUE(result.GetDict().empty());
                                   callback_called = true;
                                 }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
}

}  // namespace web
