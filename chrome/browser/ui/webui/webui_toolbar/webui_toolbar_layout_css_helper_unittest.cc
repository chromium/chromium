// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "base/test/test_future.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WebUIToolbarLayoutCssHelperTest, GenerateLayoutConstantsCss) {
  std::string css = WebUIToolbarLayoutCssHelper::GenerateLayoutConstantsCss();
  EXPECT_FALSE(css.empty());
  EXPECT_TRUE(css.find("--toolbar-button-icon-size:") != std::string::npos);
}

TEST(WebUIToolbarLayoutCssHelperTest, ShouldHandleRequest) {
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

TEST(WebUIToolbarLayoutCssHelperTest, HandleRequest) {
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

TEST(WebUIToolbarLayoutCssHelperTest, PopulateLocalResourceLoaderConfig) {
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
