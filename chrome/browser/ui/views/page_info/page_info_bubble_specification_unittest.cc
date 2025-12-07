// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_ui_types.h"
#include "url/gurl.h"

using PageInfoBubbleSpecificationTest = ChromeRenderViewHostTestHarness;

TEST_F(PageInfoBubbleSpecificationTest, DefaultSpec) {
  auto anchor_view = std::make_unique<views::View>();
  auto const test_web_contents = CreateTestWebContents();
  GURL test_url("https://www.example.com");
  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(anchor_view.get(),
                                           gfx::NativeWindow(),
                                           test_web_contents.get(), test_url)
          .Build();

  EXPECT_EQ(views::BubbleAnchor(anchor_view.get()), specification->anchor());
  EXPECT_EQ(test_web_contents.get(), specification->web_contents());
  EXPECT_EQ(test_url, specification->url());
  EXPECT_TRUE(specification->anchor_rect().IsEmpty());
  EXPECT_FALSE(specification->initialized_callback().is_null());
  EXPECT_FALSE(specification->page_info_closing_callback().is_null());
  EXPECT_TRUE(specification->show_extended_site_info());
  EXPECT_FALSE(specification->permission_page_type().has_value());
  EXPECT_FALSE(specification->show_merchant_trust_page());
  specification.reset();
  anchor_view.reset();
}

TEST_F(PageInfoBubbleSpecificationTest, InvalidSpec) {
  auto test_web_contents = CreateTestWebContents();
  GURL test_url("https://www.example.com");

  EXPECT_DEATH(PageInfoBubbleSpecification::Builder(
                   nullptr, gfx::NativeWindow(), nullptr, test_url)
                   .Build(),
               "");

  EXPECT_DEATH(
      PageInfoBubbleSpecification::Builder(nullptr, gfx::NativeWindow(),
                                           test_web_contents.get(), test_url)
          .ShowPermissionPage(ContentSettingsType::NOTIFICATIONS)
          .ShowMerchantTrustPage()
          .Build(),
      "");
}
