// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webui/webui_bundled_code_cache_map.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
#include "chrome/grit/side_panel_bookmarks_code_cache_resources.h"
#include "chrome/grit/side_panel_customize_chrome_code_cache_resources.h"
#include "chrome/grit/side_panel_reading_list_code_cache_resources.h"
#include "chrome/grit/side_panel_shared_code_cache_resources.h"
#include "chrome/grit/side_panel_tabs_from_other_devices_code_cache_resources.h"
#include "chrome/grit/webui_toolbar_code_cache_resources.h"
#include "ui/webui/resources/grit/webui_code_cache_resources.h"

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/new_tab_page_code_cache_resources.h"
#include "chrome/grit/new_tab_page_eager_code_cache_resources.h"
#include "components/search/ntp_features.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)

namespace webui {

TEST(WebUIBundledCodeCacheMapTest, RejectsInvalidAndUnsupportedUrls) {
  EXPECT_EQ(std::nullopt, GetWebUIBundledCodeCacheResourceId(GURL()));
  EXPECT_EQ(std::nullopt,
            GetWebUIBundledCodeCacheResourceId(GURL("chrome://resources/")));
  EXPECT_EQ(std::nullopt, GetWebUIBundledCodeCacheResourceId(GURL(
                              "chrome://resources/lit/v3_0/lit.rollup.js/")));
  EXPECT_EQ(std::nullopt,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://resources/lit/v3_0/lit.rollup.js.code_cache")));
  EXPECT_EQ(std::nullopt, GetWebUIBundledCodeCacheResourceId(GURL(
                              "https://resources/lit/v3_0/lit.rollup.js")));
  EXPECT_EQ(std::nullopt, GetWebUIBundledCodeCacheResourceId(GURL(
                              "chrome://settings/lit/v3_0/lit.rollup.js")));
  EXPECT_EQ(
      std::nullopt,
      GetWebUIBundledCodeCacheResourceId(GURL(
          "chrome-untrusted://webui-toolbar.top-chrome/webui_toolbar.js")));
}

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
TEST(WebUIBundledCodeCacheMapTest, FindsBundledCodeCacheResourceIds) {
  // Shared chrome://resources and chrome-untrusted://resources (including URLs
  // with query parameters or fragments).
  EXPECT_EQ(IDR_LIT_V3_0_CODE_CACHE_LIT_ROLLUP_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://resources/lit/v3_0/lit.rollup.js")));
  EXPECT_EQ(IDR_LIT_V3_0_CODE_CACHE_LIT_ROLLUP_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://resources/lit/v3_0/lit.rollup.js?foo=1")));
  EXPECT_EQ(IDR_LIT_V3_0_CODE_CACHE_LIT_ROLLUP_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://resources/lit/v3_0/lit.rollup.js#bar")));
  EXPECT_EQ(IDR_LIT_V3_0_CODE_CACHE_LIT_ROLLUP_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome-untrusted://resources/lit/v3_0/lit.rollup.js")));

  // Side panel shared and host-specific bundles.
  EXPECT_EQ(
      IDR_SIDE_PANEL_SHARED_CODE_CACHE_SP_HEADING_JS_CODE_CACHE,
      GetWebUIBundledCodeCacheResourceId(GURL(
          "chrome://bookmarks-side-panel.top-chrome/shared/sp_heading.js")));
  EXPECT_EQ(
      IDR_SIDE_PANEL_BOOKMARKS_CODE_CACHE_POWER_BOOKMARKS_APP_JS_CODE_CACHE,
      GetWebUIBundledCodeCacheResourceId(GURL(
          "chrome://bookmarks-side-panel.top-chrome/power_bookmarks_app.js")));
  EXPECT_EQ(IDR_SIDE_PANEL_SHARED_CODE_CACHE_SP_HEADING_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://customize-chrome-side-panel.top-chrome/shared/"
                     "sp_heading.js")));
  EXPECT_EQ(IDR_SIDE_PANEL_CUSTOMIZE_CHROME_CODE_CACHE_APP_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(GURL(
                "chrome://customize-chrome-side-panel.top-chrome/app.js")));
  EXPECT_EQ(IDR_SIDE_PANEL_SHARED_CODE_CACHE_SP_HEADING_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://read-later.top-chrome/shared/sp_heading.js")));
  EXPECT_EQ(
      IDR_SIDE_PANEL_READING_LIST_CODE_CACHE_READING_LIST_APP_JS_CODE_CACHE,
      GetWebUIBundledCodeCacheResourceId(
          GURL("chrome://read-later.top-chrome/reading_list_app.js")));
  EXPECT_EQ(
      IDR_SIDE_PANEL_SHARED_CODE_CACHE_SP_HEADING_JS_CODE_CACHE,
      GetWebUIBundledCodeCacheResourceId(GURL(
          "chrome://tabs-from-other-devices.top-chrome/shared/sp_heading.js")));
  EXPECT_EQ(IDR_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_CODE_CACHE_APP_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://tabs-from-other-devices.top-chrome/app.js")));

  // WebUI toolbar.
  EXPECT_EQ(IDR_WEBUI_TOOLBAR_CODE_CACHE_WEBUI_TOOLBAR_ROLLUP_JS_CODE_CACHE,
            GetWebUIBundledCodeCacheResourceId(
                GURL("chrome://webui-toolbar.top-chrome/webui_toolbar.js")));

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  // WebUI New Tab Page (lazy and eager composebox bundles).
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(
        features::kWebUIBundledCodeCacheForNewTabPage);
    EXPECT_EQ(std::nullopt, GetWebUIBundledCodeCacheResourceId(
                                GURL("chrome://new-tab-page/new_tab_page.js")));
  }
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatureStates({
        {features::kWebUIBundledCodeCacheForNewTabPage, true},
        {ntp_features::kNtpLazyLoadComposebox, true},
    });
    EXPECT_EQ(IDR_NEW_TAB_PAGE_CODE_CACHE_NEW_TAB_PAGE_ROLLUP_JS_CODE_CACHE,
              GetWebUIBundledCodeCacheResourceId(
                  GURL("chrome://new-tab-page/new_tab_page.js")));
    EXPECT_EQ(IDR_NEW_TAB_PAGE_CODE_CACHE_LAZY_LOAD_ROLLUP_JS_CODE_CACHE,
              GetWebUIBundledCodeCacheResourceId(
                  GURL("chrome://new-tab-page/lazy_load.js")));
    EXPECT_EQ(IDR_NEW_TAB_PAGE_CODE_CACHE_SHARED_ROLLUP_JS_CODE_CACHE,
              GetWebUIBundledCodeCacheResourceId(
                  GURL("chrome://new-tab-page/shared.rollup.js")));
  }
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatureStates({
        {features::kWebUIBundledCodeCacheForNewTabPage, true},
        {ntp_features::kNtpLazyLoadComposebox, false},
    });
    EXPECT_EQ(
        IDR_NEW_TAB_PAGE_EAGER_CODE_CACHE_NEW_TAB_PAGE_EAGER_ROLLUP_JS_CODE_CACHE,
        GetWebUIBundledCodeCacheResourceId(
            GURL("chrome://new-tab-page/new_tab_page.js")));
    EXPECT_EQ(IDR_NEW_TAB_PAGE_EAGER_CODE_CACHE_LAZY_LOAD_ROLLUP_JS_CODE_CACHE,
              GetWebUIBundledCodeCacheResourceId(
                  GURL("chrome://new-tab-page/lazy_load.js")));
    EXPECT_EQ(IDR_NEW_TAB_PAGE_EAGER_CODE_CACHE_SHARED_ROLLUP_JS_CODE_CACHE,
              GetWebUIBundledCodeCacheResourceId(
                  GURL("chrome://new-tab-page/shared.rollup.js")));
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
}
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)

}  // namespace webui
