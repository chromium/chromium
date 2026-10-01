// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/renderer/facilitated_payments_agent.h"

#include <string>
#include <string_view>
#include <tuple>

#include "base/strings/strcat.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

constexpr std::string_view kPaymentLinkHtml =
    R"(<link rel="facilitated-payment" href="https://psp.example/pay">)";
// "pix" and "checkout" are in the default `kQrCodeDetectionKeywords` list.
constexpr std::string_view kKeywordTitleHtml = "<title>Pay with Pix</title>";
constexpr std::string_view kNeutralTitleHtml = "<title>Store</title>";
constexpr std::string_view kSquareCanvasHtml =
    R"(<canvas width="200" height="200"></canvas>)";
constexpr std::string_view kKeywordUrl = "https://merchant.example/checkout";
constexpr std::string_view kNeutralUrl = "https://merchant.example/home";

// Exposes `CalculateHeuristicSignals()` so that it can run on a real document.
class TestFacilitatedPaymentsAgent : public FacilitatedPaymentsAgent {
 public:
  explicit TestFacilitatedPaymentsAgent(content::RenderFrame* render_frame)
      : FacilitatedPaymentsAgent(render_frame, /*registry=*/nullptr) {}

  using FacilitatedPaymentsAgent::CalculateHeuristicSignals;
};

class FacilitatedPaymentsAgentBrowserTest : public content::RenderViewTest {
 protected:
  // Loads a page built from `head` and `body` at `url`, and returns the signals
  // the agent computes for it.
  mojom::HeuristicSignalsPtr CalculateSignals(std::string_view head,
                                              std::string_view body,
                                              std::string_view url) {
    LoadHTMLWithUrlOverride(base::StrCat({"<html><head>", head, "</head><body>",
                                          body, "</body></html>"}),
                            url);
    // The agent is created after the load because a navigation may replace
    // the `RenderFrame`. The frame owns the agent, which deletes itself in
    // `OnDestruct()`.
    auto* agent = new TestFacilitatedPaymentsAgent(GetMainRenderFrame());
    return agent->CalculateHeuristicSignals();
  }

  // Returns whether a page with `body` has a QR code image candidate.
  bool HasQrCodeImageCandidate(std::string_view body) {
    return CalculateSignals(kNeutralTitleHtml, body, kNeutralUrl)
        ->has_square_candidate;
  }
};

// Test that a page with none of the signals reports all of them as false.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestNoSignals) {
  EXPECT_EQ(CalculateSignals(kNeutralTitleHtml, "", kNeutralUrl),
            mojom::HeuristicSignals::New());
}

// Test that `text_keyword_match` also covers `<meta name="description">`.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestMetaDescriptionKeyword) {
  EXPECT_TRUE(CalculateSignals(
                  base::StrCat({kNeutralTitleHtml,
                                R"(<meta name="description" content="Pix">)"}),
                  "", kNeutralUrl)
                  ->text_keyword_match);
}

// Test that keyword matching ignores case.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestKeywordMatchIsCaseInsensitive) {
  EXPECT_TRUE(CalculateSignals("<title>PIX</title>", "", kNeutralUrl)
                  ->text_keyword_match);
}

// Test that `url_keyword_match` also covers the URL query.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestUrlQueryKeyword) {
  EXPECT_TRUE(CalculateSignals(kNeutralTitleHtml, "",
                               "https://merchant.example/order?method=pix")
                  ->url_keyword_match);
}

// Test that a keyword in the host alone is not a URL match.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestUrlHostKeywordIgnored) {
  EXPECT_FALSE(
      CalculateSignals(kNeutralTitleHtml, "", "https://pix.example/home")
          ->url_keyword_match);
}

// Test that a large enough square `<img>` is a candidate.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestSquareImageIsCandidate) {
  EXPECT_TRUE(HasQrCodeImageCandidate(
      R"(<img style="display:block;width:200px;height:200px">)"));
}

// Test that a nearly square element within the aspect ratio tolerance is a
// candidate.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestNearlySquareCanvasIsCandidate) {
  EXPECT_TRUE(
      HasQrCodeImageCandidate(R"(<canvas width="110" height="100"></canvas>)"));
}

// Test that an element below the minimum size, such as an icon, is not a
// candidate.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestSmallCanvasIsNotCandidate) {
  EXPECT_FALSE(
      HasQrCodeImageCandidate(R"(<canvas width="40" height="40"></canvas>)"));
}

// Test that a wide element, such as a banner, is not a candidate.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestWideCanvasIsNotCandidate) {
  EXPECT_FALSE(
      HasQrCodeImageCandidate(R"(<canvas width="200" height="100"></canvas>)"));
}

// Test that a square element other than `<img>` or `<canvas>` is ignored.
TEST_F(FacilitatedPaymentsAgentBrowserTest, TestSquareDivIsNotCandidate) {
  EXPECT_FALSE(HasQrCodeImageCandidate(
      R"(<div style="width:200px;height:200px"></div>)"));
}

// Parameters: `has_facilitated_payment_link`, `has_square_candidate`,
// `url_keyword_match`, `text_keyword_match`.
class FacilitatedPaymentsAgentSignalCombinationTest
    : public FacilitatedPaymentsAgentBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {};

// Test that every combination of page features is reported exactly, i.e. each
// signal is computed independently and none masks another.
TEST_P(FacilitatedPaymentsAgentSignalCombinationTest,
       TestReportsEachSignalIndependently) {
  const auto [has_link, has_square, url_match, text_match] = GetParam();

  const std::string head =
      base::StrCat({has_link ? kPaymentLinkHtml : std::string_view(),
                    text_match ? kKeywordTitleHtml : kNeutralTitleHtml});
  mojom::HeuristicSignalsPtr signals = CalculateSignals(
      head, has_square ? kSquareCanvasHtml : std::string_view(),
      url_match ? kKeywordUrl : kNeutralUrl);

  mojom::HeuristicSignalsPtr expected = mojom::HeuristicSignals::New();
  expected->has_facilitated_payment_link = has_link;
  expected->has_square_candidate = has_square;
  expected->url_keyword_match = url_match;
  expected->text_keyword_match = text_match;
  EXPECT_EQ(signals, expected);
}

INSTANTIATE_TEST_SUITE_P(All,
                         FacilitatedPaymentsAgentSignalCombinationTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace
}  // namespace payments::facilitated
