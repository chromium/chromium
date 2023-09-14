// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
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
  struct StoredOnResponseCallback {
    bool was_called = false;

    void OnResponse(const TabOrganizationResponse* response) {
      was_called = true;
    }
  };

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

TEST_F(TabOrganizationTest, TabOrganizationAddingTabData) {
  TabOrganization organization({}, {u"default_name"}, 0, absl::nullopt);
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 0);

  content::WebContents* web_contents = AddTab();
  TabData tab_data(tab_strip_model(), web_contents);

  organization.AddTabData(std::move(tab_data));
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 1);
}

TEST_F(TabOrganizationTest, TabOrganizationRemovingTabData) {
  TabOrganization organization({}, {u"default_name"}, 0, absl::nullopt);
  content::WebContents* web_contents = AddTab();
  TabData tab_data(tab_strip_model(), web_contents);
  TabData::TabID tab_data_id = tab_data.tab_id();
  organization.AddTabData(std::move(tab_data));
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 1);

  organization.RemoveTabData(tab_data_id);
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 0);
}

TEST_F(TabOrganizationTest, TabOrganizationChangingCurrentName) {
  std::u16string name_0 = u"name_0";
  std::u16string name_1 = u"name_1";
  TabOrganization organization({}, {name_0, name_1}, 0, absl::nullopt);
  EXPECT_TRUE(absl::holds_alternative<size_t>(organization.current_name()));
  EXPECT_EQ(static_cast<int>(absl::get<size_t>(organization.current_name())),
            0);
  EXPECT_EQ(organization.GetDisplayName(), name_0);

  organization.SetCurrentName(1);
  EXPECT_TRUE(absl::holds_alternative<size_t>(organization.current_name()));
  EXPECT_EQ(static_cast<int>(absl::get<size_t>(organization.current_name())),
            1);
  EXPECT_EQ(organization.GetDisplayName(), name_1);

  std::u16string custom_name = u"custom_name";
  organization.SetCurrentName(custom_name);
  EXPECT_TRUE(
      absl::holds_alternative<std::u16string>(organization.current_name()));
  EXPECT_EQ((absl::get<std::u16string>(organization.current_name())),
            custom_name);
  EXPECT_EQ(organization.GetDisplayName(), custom_name);
}

TEST_F(TabOrganizationTest, TabOrganizationChangingUserActions) {
  TabOrganization accept_organization({}, {u"default_name"}, 0, absl::nullopt);

  accept_organization.Accept();
  EXPECT_EQ(accept_organization.choice(),
            TabOrganization::UserChoice::ACCEPTED);

  TabOrganization reject_organization({}, {u"default_name"}, 0, absl::nullopt);

  reject_organization.Reject();
  EXPECT_EQ(reject_organization.choice(),
            TabOrganization::UserChoice::REJECTED);
}

TEST_F(TabOrganizationTest, TabOrganizationCHECKOnChangingUserChoiceTwice) {
  TabOrganization organization({}, {u"default_name"}, 0,
                               TabOrganization::UserChoice::ACCEPTED);

  EXPECT_DEATH(organization.Reject(), "");
}

TEST_F(TabOrganizationTest, TabOrganizationRequestOnStartRequest) {
  bool start_called = false;
  TabOrganizationRequest request(
      base::BindLambdaForTesting(
          [&](const TabOrganizationRequest* request) { start_called = true; }),
      base::DoNothing());
  EXPECT_EQ(request.state(), TabOrganizationRequest::State::NOT_STARTED);

  request.StartRequest();
  EXPECT_EQ(request.state(), TabOrganizationRequest::State::STARTED);
  EXPECT_TRUE(start_called);
}

TEST_F(TabOrganizationTest,
       TabOrganizationRequestCHECKOnStartingFromStartedState) {
  TabOrganizationRequest request;
  request.StartRequest();
  EXPECT_DEATH(request.StartRequest(), "");
}

TEST_F(TabOrganizationTest, TabOrganizationRequestOnCompleteRequest) {
  TabOrganizationRequest request;

  StoredOnResponseCallback stored_callback;
  request.SetResponseCallback(
      base::BindOnce(&StoredOnResponseCallback::OnResponse,
                     base::Unretained(&stored_callback)));
  request.StartRequest();
  request.CompleteRequestForTesting({});
  EXPECT_EQ(request.state(), TabOrganizationRequest::State::COMPLETED);
  EXPECT_TRUE(stored_callback.was_called);
}

TEST_F(TabOrganizationTest, TabOrganizationRequestOnFailRequest) {
  TabOrganizationRequest request;
  request.StartRequest();
  request.FailRequest();
  EXPECT_EQ(request.state(), TabOrganizationRequest::State::FAILED);
}

TEST_F(TabOrganizationTest, TabOrganizationRequestOnCancelRequest) {
  bool cancel_called = false;
  TabOrganizationRequest request(
      base::DoNothing(),
      base::BindLambdaForTesting([&](const TabOrganizationRequest* request) {
        cancel_called = true;
      }));
  request.StartRequest();
  request.CancelRequest();
  EXPECT_EQ(request.state(), TabOrganizationRequest::State::CANCELED);
  EXPECT_TRUE(cancel_called);
}
