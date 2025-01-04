// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/border/border_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace glic {

namespace {
class BorderViewBrowserTest : public InProcessBrowserTest {
 public:
  BorderViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  ~BorderViewBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGlicGuestURL,
                                    url::kAboutBlankURL);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace

// Exercise that, the border is resized correctly whenever the browser's size
// changes.
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, BorderResize) {
  // TODO(crbug.com/385828490): We should exercise the proper closing flow.
  // Currently the BookmarkModel has a dangling observer during destruction, if
  // the glic UI is toggled.
  auto* border =
      browser()->window()->AsBrowserView()->contents_web_view()->glic_border();
  ASSERT_TRUE(border);
  border->StartAnimation();
  auto* contents_web_view = browser()->GetBrowserView().contents_web_view();
  EXPECT_EQ(border->GetVisibleBounds(), contents_web_view->GetVisibleBounds());

  // Note: there is a minimal size that the desktop window can be. It seems to
  // be around 500px by 500px.
  const gfx::Size new_size(600, 600);
  auto* browser_window = browser()->window();
  const gfx::Rect new_bounds(browser_window->GetBounds().origin(), new_size);
  EXPECT_NE(browser_window->GetBounds(), new_bounds);

  {
    SCOPED_TRACE("resizing");
    browser_window->SetBounds(new_bounds);
    content::RunAllPendingInMessageLoop();
  }

  // Resized correctly.
  EXPECT_EQ(browser_window->GetBounds(), new_bounds);
  EXPECT_EQ(border->GetVisibleBounds(), contents_web_view->GetVisibleBounds());
}

// Regression test for https://crbug.com/387458471: The border shouldn't be
// visible before StartAnimation is called, and shouldn't be visible after
// CancelAnimation is called.
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, Visibility) {
  auto* border =
      browser()->window()->AsBrowserView()->contents_web_view()->glic_border();
  ASSERT_TRUE(border);
  EXPECT_FALSE(border->GetVisible());
  border->StartAnimation();
  EXPECT_TRUE(border->GetVisible());
  border->CancelAnimation();
  EXPECT_FALSE(border->GetVisible());
}

namespace {
class BorderViewFeatureDisabledBrowserTest : public BorderViewBrowserTest {
 public:
  BorderViewFeatureDisabledBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(features::kGlic);
  }
  ~BorderViewFeatureDisabledBrowserTest() override = default;
};
}  // namespace

// Regression test for https://crbug.com/387458471: The border is not
// initialized if the feature is disabled.
IN_PROC_BROWSER_TEST_F(BorderViewFeatureDisabledBrowserTest, NoBorder) {
  auto* border =
      browser()->window()->AsBrowserView()->contents_web_view()->glic_border();
  EXPECT_FALSE(border);
}

namespace {

class BorderViewZOrderBrowserTest : public BorderViewBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  BorderViewZOrderBrowserTest() = default;
  ~BorderViewZOrderBrowserTest() override = default;
};

// Another View that also fights to be the z-topmost child.
class CompetingSibling : public views::View, public views::ViewObserver {
  METADATA_HEADER(CompetingSibling, View)
 public:
  explicit CompetingSibling(views::View* parent) : parent_(parent) {
    parent_->AddObserver(this);
  }
  CompetingSibling(const CompetingSibling&) = delete;
  CompetingSibling& operator=(const CompetingSibling&) = delete;
  ~CompetingSibling() override { parent_->RemoveObserver(this); }

  // `views::ViewObserver`:
  void OnChildViewReordered(views::View* observed_view,
                            views::View* child) override {
    ASSERT_EQ(observed_view, parent_);
    parent_->ReorderChildView(this, parent_->children().size());
  }

 private:
  const raw_ptr<views::View> parent_ = nullptr;
};

BEGIN_METADATA(CompetingSibling)
END_METADATA

}  // namespace

// The glic border should always be the z-topmost child of the contents web
// view, unless there is a competing sibling.
IN_PROC_BROWSER_TEST_P(BorderViewZOrderBrowserTest, TopMostChild) {
  auto* parent = browser()->GetBrowserView().contents_web_view();
  ASSERT_TRUE(parent);
  auto* border = static_cast<views::View*>(parent->glic_border());
  ASSERT_TRUE(border);

  EXPECT_EQ(border, parent->children().back());
  EXPECT_EQ(parent->children().size(), 2u);

  views::View* existing_sibling = parent->children().at(0u);
  views::View* new_sibling = nullptr;

  bool competing_sibling = GetParam();

  EXPECT_EQ(parent->children().size(), 2u);
  new_sibling = parent->AddChildViewAt(
      competing_sibling ? std::make_unique<CompetingSibling>(parent)
                        : std::make_unique<views::View>(),
      2u);
  if (competing_sibling) {
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, border, new_sibling));
  } else {
    // If the border view has another sibling that also competes to be the
    // z-topmost child, the border view should back down and not lock up the UI
    // thread.
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, new_sibling, border));
  }

  EXPECT_EQ(parent->children().size(), 3u);
  views::View* reorder_target = parent->children().at(1u);
  parent->ReorderChildView(reorder_target, 3u);
  if (competing_sibling) {
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, border, new_sibling));
  } else {
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, new_sibling, border));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BorderViewZOrderBrowserTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<BorderViewZOrderBrowserTest::ParamType>&
           info) {
      return info.param ? "CompetingSibling" : "RegularSibling";
    });

}  // namespace glic
