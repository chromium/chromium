// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/wallet/boarding_pass_extractor.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

class BoardingPassExtractorBrowserTest : public ChromeRenderViewTest {
 public:
};

TEST_F(BoardingPassExtractorBrowserTest, ExtractBoardingPassWithScript) {
  auto extractor_ = std::make_unique<BoardingPassExtractor>(
      GetMainRenderFrame(), registry_.get());

  const char* html = R"HTML(
      <html>
        <head>
          <title>Google Boarding Pass Page</title>
        </head>
        <body>
        </body>
      </html>
    )HTML";
  LoadHTML(html);

  const std::string script = R"###(
    results = ['M1TEST', 'M2TEST']
  )###";

  extractor_->ExtractBoardingPassWithScript(
      script,
      base::BindOnce([](const std::vector<std::string>& boarding_passes) {
        EXPECT_EQ(boarding_passes.size(), 2u);
        EXPECT_EQ(boarding_passes[0], "M1TEST");
        EXPECT_EQ(boarding_passes[1], "M2TEST");
      }));
  base::RunLoop().RunUntilIdle();
}

TEST_F(BoardingPassExtractorBrowserTest, ExtractBoardingPassTitleNotMatch) {
  auto extractor_ = std::make_unique<BoardingPassExtractor>(
      GetMainRenderFrame(), registry_.get());

  const char* html = R"HTML(
      <html>
        <head>
          <title>Google Testing Page</title>
        </head>
        <body>
        </body>
      </html>
    )HTML";
  LoadHTML(html);

  extractor_->ExtractBoardingPass(
      base::BindOnce([](const std::vector<std::string>& boarding_passes) {
        EXPECT_EQ(boarding_passes.size(), 0u);
      }));
  base::RunLoop().RunUntilIdle();
}

}  // namespace wallet
