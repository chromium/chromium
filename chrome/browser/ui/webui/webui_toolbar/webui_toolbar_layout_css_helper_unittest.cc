// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/style/typography.h"
#include "ui/views/test/test_layout_provider.h"

class WebUIToolbarLayoutCssHelperTest : public testing::Test {
 protected:
  WebUIToolbarLayoutCssHelperTest() {
    layout_provider_.SetFontDetails(
        CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY,
        ui::ResourceBundle::FontDetails("Globe Old Greek", /*size_delta=*/0,
                                        /*weight=*/gfx::Font::Weight::NORMAL));
  }

  views::test::TestLayoutProvider layout_provider_;
};

TEST_F(WebUIToolbarLayoutCssHelperTest, GenerateLayoutConstantsCss) {
  std::string css = WebUIToolbarLayoutCssHelper::GenerateLayoutConstantsCss();
  EXPECT_FALSE(css.empty());
  EXPECT_TRUE(css.find("--toolbar-button-icon-size:") != std::string::npos);

  // We can't actually test the font family (since the platform will substitute
  // something real), and size isn't controlled here, but weight is at least
  // predictable.
  EXPECT_TRUE(css.find("--omnibox-primary-font-family:") != std::string::npos);
  EXPECT_TRUE(css.find("--omnibox-primary-font-weight:400;") !=
              std::string::npos);
}

TEST_F(WebUIToolbarLayoutCssHelperTest, ShouldHandleRequest) {
  EXPECT_FALSE(WebUIToolbarLayoutCssHelper::ShouldHandleRequest("lc.css"));
  EXPECT_FALSE(WebUIToolbarLayoutCssHelper::ShouldHandleRequest(""));
  EXPECT_TRUE(
      WebUIToolbarLayoutCssHelper::ShouldHandleRequest("layout_constants.css"));
  EXPECT_TRUE(WebUIToolbarLayoutCssHelper::ShouldHandleRequest(
      "layout_constants_v0.css"));
  EXPECT_TRUE(WebUIToolbarLayoutCssHelper::ShouldHandleRequest(
      "layout_constants_v1.css"));
  EXPECT_TRUE(WebUIToolbarLayoutCssHelper::ShouldHandleRequest(
      "layout_constantswhatever.css"));
  // We don't expect query params to be used.
  EXPECT_FALSE(WebUIToolbarLayoutCssHelper::ShouldHandleRequest(
      "layout_constants.css?v=0"));
  // Must have a proper extension.
  EXPECT_FALSE(WebUIToolbarLayoutCssHelper::ShouldHandleRequest(
      "layout_constants.cssilly"));
}

TEST_F(WebUIToolbarLayoutCssHelperTest, HandleRequest) {
  base::test::TestFuture<scoped_refptr<base::RefCountedMemory>> future;
  WebUIToolbarLayoutCssHelper::HandleRequest("ignored", future.GetCallback());
  auto result = future.Get();
  ASSERT_TRUE(result);
  auto as_span = base::as_byte_span(*result);
  std::string as_string(as_span.begin(), as_span.end());
  EXPECT_FALSE(as_string.empty());
  EXPECT_TRUE(as_string.find("--toolbar-button-icon-size:") !=
              std::string::npos);
}

TEST_F(WebUIToolbarLayoutCssHelperTest, PopulateLocalResourceLoaderConfig) {
  blink::mojom::LocalResourceLoaderConfigPtr local_config =
      blink::mojom::LocalResourceLoaderConfig::New();
  WebUIToolbarLayoutCssHelper::PopulateLocalResourceLoaderConfig(
      local_config.get());
  ASSERT_EQ(1u, local_config->sources.size());
  auto it = local_config->sources.begin();
  EXPECT_EQ(it->first,
            url::Origin::Create(GURL(chrome::kChromeUIWebUIToolbarURL)));
  ASSERT_TRUE(it->second);

  ASSERT_EQ(1u, it->second->path_to_resource_map.size());
  auto it2 = it->second->path_to_resource_map.begin();
  EXPECT_EQ("layout_constants_v0.css", it2->first);
  ASSERT_TRUE(it2->second);
  ASSERT_TRUE(it2->second->is_response_body());
  EXPECT_EQ(WebUIToolbarLayoutCssHelper::GenerateLayoutConstantsCss(),
            it2->second->get_response_body());
}

TEST_F(WebUIToolbarLayoutCssHelperTest, EscapeCssFontName) {
  EXPECT_EQ("Greek", WebUIToolbarLayoutCssHelper::EscapeCssFontName("Greek"));
  EXPECT_EQ("Gr\\\\eek",
            WebUIToolbarLayoutCssHelper::EscapeCssFontName("Gr\\eek"));
  EXPECT_EQ("\\\"Greek",
            WebUIToolbarLayoutCssHelper::EscapeCssFontName("\"Greek"));
  EXPECT_EQ("\\0A \\0C ",
            WebUIToolbarLayoutCssHelper::EscapeCssFontName("\n\f"));
}
