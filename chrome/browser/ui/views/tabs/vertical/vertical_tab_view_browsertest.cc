// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_icon.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/label.h"

class VerticalTabViewTest : public InProcessBrowserTest {
 public:
  VerticalTabViewTest() = default;
  ~VerticalTabViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({tabs::kVerticalTabs}, {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, TitleDataChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Create view hierarchy from an arbitrary parent view since we don't
  // currently support updates from the API.
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));
  views::Label* title =
      BrowserElementsViews::From(browser())->GetViewAs<views::Label>(
          kVerticalTabTitleElementId);

  // Expect the initial title to match the one in content/test/data/title2.html
  EXPECT_EQ(u"Title Of Awesomeness", title->GetText());

  // After navigating, expect title to be updated and match the one in
  // content/test/data/title3.html
  GURL changed_url = embedded_test_server()->GetURL("/title3.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), changed_url));
  EXPECT_EQ(u"Title Of More Awesomeness", title->GetText());
}

IN_PROC_BROWSER_TEST_F(VerticalTabViewTest, IconDataChanged) {
  // Create view hierarchy from an arbitrary parent view since we don't
  // currently support updates from the API.
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The initial tab is the first child of the unpinned collection which is the
  // second child of the root node.
  TabCollectionNode* tab_node = root_node.children()[1]->children()[0].get();
  VerticalTabIcon* icon =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing())
          ->icon_for_testing();

  // Expect the favicon to not be loading initially.
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());

  // After changing network state, expect the favicon to be loading.
  tabs_api::mojom::DataPtr tab_data = tab_node->data()->Clone();
  tab_data->get_tab()->network_state = tabs_api::mojom::NetworkState::kLoading;
  auto event = tabs_api::mojom::OnDataChangedEvent::New();
  event->data = std::move(tab_data);
  root_node.OnDataChanged(event);
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
}
