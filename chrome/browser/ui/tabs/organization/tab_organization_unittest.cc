// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabOrganizationTest : public testing::Test {
 public:
  TabOrganizationTest()
      : profile_(new TestingProfile),
        delegate_(new TestTabStripModelDelegate),
        tab_strip_model_(new TabStripModel(delegate(), profile())) {}

  TestingProfile* profile() { return profile_.get(); }
  TestTabStripModelDelegate* delegate() { return delegate_.get(); }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

  content::WebContents* AddTab() {
    std::unique_ptr<content::WebContents> contents_unique_ptr =
        CreateWebContents();
    content::WebContents* content_ptr = contents_unique_ptr.get();
    tab_strip_model()->AppendWebContents(std::move(contents_unique_ptr), true);

    return content_ptr;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;

  const std::unique_ptr<TestTabStripModelDelegate> delegate_;
  const std::unique_ptr<TabStripModel> tab_strip_model_;
};

// The constructor that takes the webcontents and tabstrip model should
// instantiate correctly.
TEST_F(TabOrganizationTest, TabDataTabStripModelConstructor) {
  content::WebContents* web_contents = AddTab();
  TabData tab_data(tab_strip_model(), web_contents);
  EXPECT_EQ(tab_strip_model(), tab_data.original_tab_strip_model());
  EXPECT_EQ(web_contents->GetLastCommittedURL(), tab_data.original_url());
  EXPECT_EQ(tab_strip_model()->GetIndexOfWebContents(web_contents),
            tab_data.original_index());

  // TODO(1476012) Add a check for TabID once TabStripModel::Tab has the new
  // handle.
}

// Check that TabData isn't updated when the tabstrip updates.
TEST_F(TabOrganizationTest, TabDataTabStripTabUpdatingDoesntUpdateTabData) {
  content::WebContents* web_contents = AddTab();
  GURL old_gurl = GURL("chrome://page_1");
  content::WebContentsTester::For(web_contents)->NavigateAndCommit(old_gurl);

  TabData tab_data(tab_strip_model(), web_contents);

  // When updating tab URL, the TabData shouldn't update.
  content::WebContentsTester::For(web_contents)
      ->NavigateAndCommit(GURL("chrome://page_2"));
  EXPECT_NE(tab_data.original_url(), web_contents->GetLastCommittedURL());

  // Add an extra tab so that there's room to move.
  int current_index = tab_strip_model()->GetIndexOfWebContents(web_contents);
  AddTab();
  tab_strip_model()->MoveWebContentsAt(current_index, current_index + 1, false);
  EXPECT_EQ(tab_data.original_index(), current_index);
  EXPECT_NE(tab_data.original_index(),
            tab_strip_model()->GetIndexOfWebContents(web_contents));
}
