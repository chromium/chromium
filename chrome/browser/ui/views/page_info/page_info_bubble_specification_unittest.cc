// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"

#include <memory>

#include "base/test/bind.h"
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
      PageInfoBubbleSpecification::Builder(
          views::BubbleAnchor(anchor_view.get()), gfx::NativeWindow(),
          test_web_contents.get(), test_url)
          .Build();

  EXPECT_EQ(anchor_view.get(), specification->anchor().GetIfView());
  EXPECT_EQ(test_web_contents.get(), specification->web_contents());
  EXPECT_EQ(test_url, specification->url());
  EXPECT_TRUE(specification->anchor_rect().IsEmpty());
  EXPECT_FALSE(specification->initialized_callback().is_null());
  EXPECT_FALSE(specification->page_info_closing_callback().is_null());
  EXPECT_FALSE(specification->get_browser_callback().is_null());
  EXPECT_TRUE(specification->show_extended_site_info());
  EXPECT_FALSE(specification->permission_page_type().has_value());
  EXPECT_FALSE(specification->show_extensions_menu());
  specification.reset();
  anchor_view.reset();
}

TEST_F(PageInfoBubbleSpecificationTest, ShowExtensionsMenuSpec) {
  auto anchor_view = std::make_unique<views::View>();
  auto const test_web_contents = CreateTestWebContents();
  GURL test_url("https://www.example.com");
  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(
          views::BubbleAnchor(anchor_view.get()), gfx::NativeWindow(),
          test_web_contents.get(), test_url)
          .SetShowExtensionsMenu(true)
          .Build();

  EXPECT_TRUE(specification->show_extensions_menu());
}

TEST_F(PageInfoBubbleSpecificationTest, InvalidSpec) {
  GURL test_url("https://www.example.com");

  EXPECT_DEATH(
      PageInfoBubbleSpecification::Builder(
          views::BubbleAnchor(), gfx::NativeWindow(), nullptr, test_url)
          .Build(),
      "");
}

TEST_F(PageInfoBubbleSpecificationTest,
       CustomGetBrowserCallbackInvokedWhenConfigured) {
  auto anchor_view = std::make_unique<views::View>();
  auto const test_web_contents = CreateTestWebContents();
  GURL test_url("https://www.example.com");

  bool callback_called = false;
  ChromePageInfoDelegate::GetBrowserCallback callback =
      base::BindLambdaForTesting(
          [&](content::WebContents* contents) -> BrowserWindowInterface* {
            callback_called = true;
            return nullptr;
          });

  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(
          views::BubbleAnchor(anchor_view.get()), gfx::NativeWindow(),
          test_web_contents.get(), test_url)
          .AddGetBrowserCallback(callback)
          .Build();

  EXPECT_FALSE(specification->get_browser_callback().is_null());
  specification->get_browser_callback().Run(test_web_contents.get());
  EXPECT_TRUE(callback_called);
}
