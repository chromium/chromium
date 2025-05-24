// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/optimization_guide/optimization_guide_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/icon_view_metadata.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {

class OptimizationGuideIconViewTestBase : public InProcessBrowserTest {
 public:
  OptimizationGuideIconViewTestBase() = default;
  OptimizationGuideIconViewTestBase(const OptimizationGuideIconViewTestBase&) =
      delete;
  OptimizationGuideIconViewTestBase& operator=(
      const OptimizationGuideIconViewTestBase&) = delete;
  ~OptimizationGuideIconViewTestBase() override = default;

  void SetUpEnabledHint(const GURL& url, const std::string& label) {
    optimization_guide::proto::OptimizationGuideIconViewMetadata
        icon_view_metadata;
    icon_view_metadata.set_cue_label(label);
    optimization_guide::OptimizationMetadata metadata;
    metadata.SetAnyMetadataForTesting(icon_view_metadata);
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddHintForTesting(
            url, optimization_guide::proto::OPTIMIZATION_GUIDE_ICON_VIEW,
            metadata);
  }

  OptimizationGuideIconView* optimization_guide_icon_view() {
    views::View* const icon_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kOptimizationGuideChipElementId,
            browser()->window()->GetElementContext());
    return icon_view ? views::AsViewClass<OptimizationGuideIconView>(icon_view)
                     : nullptr;
  }
};

class OptimizationGuideIconViewTest : public OptimizationGuideIconViewTestBase {
 public:
  OptimizationGuideIconViewTest() {
    feature_list_.InitAndEnableFeature(
        optimization_guide::features::kOptimizationGuideIconView);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideIconViewTest,
                       DoesNotShowOnDisabledPage) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://disabled.com/")));

  OptimizationGuideIconView* icon_view = optimization_guide_icon_view();
  EXPECT_FALSE(icon_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideIconViewTest, ShowsOnEnabledPage) {
  // Set up enabled hint and navigate to an enabled page.
  const GURL url = GURL("https://enabled.com/");
  SetUpEnabledHint(url, "label");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  OptimizationGuideIconView* icon_view = optimization_guide_icon_view();
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_EQ(icon_view->GetText(), u"label");

  // Now, navigate to disabled page. View should not be visible.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://disabled.com/")));

  EXPECT_FALSE(icon_view->GetVisible());

  // Navigate to another enabled page - make sure label is dynamic.
  const GURL url2 = GURL("https://enabled.com/different");
  SetUpEnabledHint(url2, "anotherlabel");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_EQ(icon_view->GetText(), u"anotherlabel");
}

class OptimizationGuideIconViewDisabledTest
    : public OptimizationGuideIconViewTestBase {
 public:
  OptimizationGuideIconViewDisabledTest() {
    feature_list_.InitAndDisableFeature(
        optimization_guide::features::kOptimizationGuideIconView);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideIconViewDisabledTest,
                       NotCreatedWhenDisabled) {
  // Set up enabled hint and navigate to an enabled page.
  const GURL url = GURL("https://enabled.com/");
  SetUpEnabledHint(url, "label");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  OptimizationGuideIconView* icon_view = optimization_guide_icon_view();
  ASSERT_FALSE(icon_view);
}

}  // namespace
