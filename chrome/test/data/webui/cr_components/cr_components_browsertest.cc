// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest CrComponentsTest;

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ColorChangeListener) {
  RunTest("cr_components/color_change_listener_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, CustomizeColorSchemeMode) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/customize_color_scheme_mode_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HelpBubbleMixin) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/help_bubble/help_bubble_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HelpBubbleMixinLit) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/help_bubble/help_bubble_mixin_lit_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HelpBubble) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/help_bubble/help_bubble_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HorizontalCarousel) {
  RunTest("cr_components/history_clusters/horizontal_carousel_test.js",
          "mocha.run()");
}

class CrComponentsHistoryEmbeddingsTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsHistoryEmbeddingsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history_embeddings::kHistoryEmbeddings);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryEmbeddingsTest, HistoryEmbeddings) {
  RunTest("cr_components/history_embeddings/history_embeddings_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryEmbeddingsTest,
                       HistoryEmbeddingsFilterChips) {
  RunTest("cr_components/history_embeddings/filter_chips_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryEmbeddingsTest,
                       HistoryEmbeddingsResultImage) {
  RunTest("cr_components/history_embeddings/result_image_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ManagedDialog) {
  RunTest("cr_components/managed_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ManagedFootnote) {
  // Loaded from chrome://settings because it needs access to chrome.send().
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/managed_footnote_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, LocalizedLink) {
  RunTest("cr_components/localized_link_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsSearchboxTest;
IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, RealboxMatchTest) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/searchbox/searchbox_match_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, RealboxTest) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/searchbox/searchbox_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, RealboxLensTest) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/searchbox/searchbox_lens_test.js", "mocha.run()");
}

class CrComponentsHistoryClustersTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsHistoryClustersTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history_clusters::internal::kJourneysImages);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersTest, All) {
  RunTest("cr_components/history_clusters/history_clusters_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersTest, Cluster) {
  RunTest("cr_components/history_clusters/cluster_test.js", "mocha.run()");
}

class CrComponentsMostVisitedTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsMostVisitedTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, General) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('General');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, Layouts) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ReflowLayouts) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Reflow Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, LoggingAndUpdates) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('LoggingAndUpdates');");
}

// crbug.com/1226996
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_Modification DISABLED_Modification
#else
#define MAYBE_Modification Modification
#endif
IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, MAYBE_Modification) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Modification');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, DragAndDrop) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('DragAndDrop');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, Theming) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Theming');");
}

typedef WebUIMochaBrowserTest CrComponentsThemeColorPickerTest;
IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, ThemeColor) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/theme_color_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, CheckMarkWrapper) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/check_mark_wrapper_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, ThemeColorPicker) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/theme_color_picker_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, ThemeHueSliderDialog) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/theme_hue_slider_dialog_test.js",
          "mocha.run()");
}

class CrComponentsPrerenderTest : public CrComponentsMostVisitedTest {
 protected:
  CrComponentsPrerenderTest() {
    const std::map<std::string, std::string> params = {
        {"prerender_start_delay_on_mouse_hover_ms", "0"},
        {"preconnect_start_delay_on_mouse_hover_ms", "0"},
        {"prerender_new_tab_page_on_mouse_pressed_trigger", "true"},
        {"prerender_new_tab_page_on_mouse_hover_trigger", "true"}};
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kNewTabPageTriggerForPrerender2, params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsPrerenderTest, Prerendering) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Prerendering');");
}
