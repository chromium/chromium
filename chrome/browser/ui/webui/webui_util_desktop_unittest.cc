// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_util_desktop.h"

#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/resource_path.h"
#include "url/gurl.h"

TEST(WebUIUtilDesktopTest,
     AppendWebUIResourceURLToCodeCachePairs_AppendsPairs) {
  constexpr webui::ResourcePath kTestResources[] = {
      {"resource_1.js.code_cache", 1},
      {"resource_2.js.code_cache", 2},
      {"path/resource_3.js.code_cache", 3},
  };

  std::vector<std::pair<GURL, int>> url_to_code_cache_pairs;
  AppendWebUIResourceURLToCodeCachePairs("chrome", "test", kTestResources,
                                         url_to_code_cache_pairs);

  EXPECT_EQ(3u, url_to_code_cache_pairs.size());
  EXPECT_TRUE(base::Contains(
      url_to_code_cache_pairs,
      std::pair<GURL, int>(GURL("chrome://test/resource_1.js"), 1)));
  EXPECT_TRUE(base::Contains(
      url_to_code_cache_pairs,
      std::pair<GURL, int>(GURL("chrome://test/resource_2.js"), 2)));
  EXPECT_TRUE(base::Contains(
      url_to_code_cache_pairs,
      std::pair<GURL, int>(GURL("chrome://test/path/resource_3.js"), 3)));
}

TEST(WebUIUtilDesktopTest,
     AppendWebUIResourceURLToCodeCachePairs_EmptyCodeCacheResources) {
  std::vector<std::pair<GURL, int>> url_to_code_cache_pairs;
  AppendWebUIResourceURLToCodeCachePairs(
      "chrome", "test", base::span<const webui::ResourcePath>(),
      url_to_code_cache_pairs);

  EXPECT_TRUE(url_to_code_cache_pairs.empty());
}
