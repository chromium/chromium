// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_test.h"
#include "components/commerce/content/renderer/commerce_web_extractor.h"
#include "components/commerce/core/commerce_constants.h"

namespace {
class CommerceWebExtractorTest : public ChromeRenderViewTest {
 public:
  CommerceWebExtractorTest() = default;
};

TEST_F(CommerceWebExtractorTest, TestValidMetaExtraction) {
  std::unique_ptr<commerce::CommerceWebExtractor> extractor =
      std::make_unique<commerce::CommerceWebExtractor>(GetMainRenderFrame(),
                                                       registry_.get());
  LoadHTML(
      "<html>"
      "<head>"
      "<meta content=\"product\" property=\"og:type\">"
      "<meta content=\"product\" property=\"og:type\">"
      "</head>"
      "<body>"
      "</body></html>");

  extractor->ExtractMetaInfo(base::BindOnce([](base::Value result) {
    ASSERT_TRUE(result.is_dict());
    auto* str = result.GetDict().FindString(commerce::kOgType);
    ASSERT_TRUE(str);
    ASSERT_EQ(*str, commerce::kOgTypeOgProduct);
  }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(CommerceWebExtractorTest, TestInvalidMetaExtraction) {
  std::unique_ptr<commerce::CommerceWebExtractor> extractor =
      std::make_unique<commerce::CommerceWebExtractor>(GetMainRenderFrame(),
                                                       registry_.get());
  LoadHTML(
      "<html>"
      "<head>"
      "<meta content=\"product\" property=\"type\">"
      "<meta content=\"product\" type=\"og:type\">"
      "</head>"
      "<body>"
      "</body></html>");

  extractor->ExtractMetaInfo(base::BindOnce([](base::Value result) {
    ASSERT_TRUE(result.is_dict());
    ASSERT_TRUE(result.GetDict().empty());
  }));
  base::RunLoop().RunUntilIdle();
}
}  // namespace
