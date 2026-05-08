// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_dom_utils.h"

#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace safe_browsing {

class PhishingDOMUtilsTest : public content::RenderViewTest {
 public:
  void LoadHtml(const GURL& url, const std::string& content) {
    LoadHTMLWithUrlOverride(content.c_str(), url.spec().c_str());
  }
};

TEST_F(PhishingDOMUtilsTest, CanPerformPhishingDetection_ValidUrlAndMethod) {
  LoadHtml(GURL("http://host.net"), "<html><body>content</body></html>");
  EXPECT_EQ(PhishingProcessStatus::kValid,
            CanPerformPhishingDetection(GetMainFrame()));

  LoadHtml(GURL("https://host.net"), "<html><body>content</body></html>");
  EXPECT_EQ(PhishingProcessStatus::kValid,
            CanPerformPhishingDetection(GetMainFrame()));
}

TEST_F(PhishingDOMUtilsTest, CanPerformPhishingDetection_InvalidScheme) {
  LoadHtml(GURL("file://host.net"), "<html><body>content</body></html>");
  EXPECT_EQ(PhishingProcessStatus::kInvalidUrlFormat,
            CanPerformPhishingDetection(GetMainFrame()));

  LoadHtml(GURL("data:text/html,content"), "<html><body>content</body></html>");
  EXPECT_EQ(PhishingProcessStatus::kInvalidUrlFormat,
            CanPerformPhishingDetection(GetMainFrame()));
}

// Note: Testing different HTTP methods (e.g., POST) is difficult in
// RenderViewTest without more complex setup, but the GET check is
// part of the logic being refactored.

}  // namespace safe_browsing
