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
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "tab_organization_session.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  GURL GetUniqueTestURL() {
    static int offset = 1;
    GURL url("chrome://page_" + base::NumberToString(offset));
    offset++;
    return url;
  }

  content::WebContents* AddTab(TabStripModel* tab_strip_model = nullptr) {
    std::unique_ptr<content::WebContents> contents_unique_ptr =
        CreateWebContents();
    content::WebContentsTester::For(contents_unique_ptr.get())
        ->NavigateAndCommit(GURL(GetUniqueTestURL()));
    content::WebContents* content_ptr = contents_unique_ptr.get();
    if (!tab_strip_model) {
      tab_strip_model = tab_strip_model_.get();
    }
    tab_strip_model->AppendWebContents(std::move(contents_unique_ptr), true);

    return content_ptr;
  }

  void InvalidateTabData(TabData* tab_data) {
    // TabData is invalidated from new URL which are different from their
    // original URL. as long as the original URL was created via
    // GetUniqueTestURL this will invalidate.
    content::WebContentsTester::For(tab_data->web_contents())
        ->NavigateAndCommit(GetUniqueTestURL());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;

  const std::unique_ptr<TestTabStripModelDelegate> delegate_;
  const std::unique_ptr<TabStripModel> tab_strip_model_;
};

// TabData tests.

// The constructor that takes the webcontents and tabstrip model should
// instantiate correctly.
TEST_F(TabOrganizationTest, TabDataTabStripModelConstructor) {
  content::WebContents* web_contents = AddTab();
  TabData tab_data(tab_strip_model(), web_contents);
  EXPECT_EQ(tab_strip_model(), tab_data.original_tab_strip_model());
  EXPECT_EQ(web_contents->GetLastCommittedURL(), tab_data.original_url());

  // TODO(1476012) Add a check for TabID once TabStripModel::Tab has the new
  // handle.
}

// Check that TabData isn't updated when the tabstrip updates.
TEST_F(TabOrganizationTest, TabDataTabStripTabUpdatingURL) {
  content::WebContents* web_contents = AddTab();
  GURL old_gurl = GURL(GetUniqueTestURL());
  content::WebContentsTester::For(web_contents)->NavigateAndCommit(old_gurl);

  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(tab_strip_model(), web_contents);

  // When updating tab URL, the TabData shouldn't update.
  content::WebContentsTester::For(web_contents)
      ->NavigateAndCommit(GURL(GetUniqueTestURL()));
  EXPECT_NE(tab_data->original_url(), web_contents->GetLastCommittedURL());
}

TEST_F(TabOrganizationTest, TabDataOnTabStripModelDestroyed) {
  // Create a destroyable tabstripmodel.
  std::unique_ptr<TabStripModel> new_tab_strip_model =
      std::make_unique<TabStripModel>(delegate(), profile());

  // Create a tab data that should be listening to the tabstrip model.
  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(
      new_tab_strip_model.get(), AddTab(new_tab_strip_model.get()));

  // destroy the tabstripmodel. expect that the original tab strip model is
  // nullptr.
  EXPECT_EQ(tab_data->original_tab_strip_model(), new_tab_strip_model.get());
  new_tab_strip_model.reset();
  EXPECT_EQ(tab_data->original_tab_strip_model(), nullptr);
}

TEST_F(TabOrganizationTest, TabDataOnDestroyWebContentsSetToNull) {
  content::WebContents* web_contents = AddTab();

  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(tab_strip_model(), web_contents);

  tab_strip_model()->CloseWebContentsAt(
      tab_strip_model()->GetIndexOfWebContents(web_contents),
      TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(tab_data->web_contents(), nullptr);
}

TEST_F(TabOrganizationTest, TabDataOnDestroyWebContentsReplaceUpdatesContents) {
  content::WebContents* old_contents = AddTab();

  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(tab_strip_model(), old_contents);

  std::unique_ptr<content::WebContents> new_contents = CreateWebContents();
  content::WebContents* new_contents_ptr = new_contents.get();
  EXPECT_EQ(tab_data->web_contents(), old_contents);
  tab_strip_model()->ReplaceWebContentsAt(
      tab_strip_model()->GetIndexOfWebContents(old_contents),
      std::move(new_contents));
  EXPECT_EQ(tab_data->web_contents(), new_contents_ptr);
}

TEST_F(TabOrganizationTest, TabDataURLChangeIsNotValidForOrganizing) {
  content::WebContents* web_contents = AddTab();
  GURL old_gurl = GURL(GetUniqueTestURL());
  content::WebContentsTester::For(web_contents)->NavigateAndCommit(old_gurl);

  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(tab_strip_model(), web_contents);

  EXPECT_TRUE(tab_data->IsValidForOrganizing());

  // update the URL for the webcontents, expect the tab data to not be valid.
  // When updating tab URL, the TabData shouldn't update.
  content::WebContentsTester::For(tab_data->web_contents())
      ->NavigateAndCommit(GetUniqueTestURL());
  EXPECT_FALSE(tab_data->IsValidForOrganizing());
}

TEST_F(TabOrganizationTest, TabDataWebContentsDeletionIsNotValidForOrganizing) {
  content::WebContents* web_contents = AddTab();
  GURL old_gurl = GURL(GetUniqueTestURL());
  content::WebContentsTester::For(web_contents)->NavigateAndCommit(old_gurl);

  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(tab_strip_model(), web_contents);
  EXPECT_TRUE(tab_data->IsValidForOrganizing());

  // Add a new tab so that the tabstripmodel doesnt close.
  AddTab();

  // Delete the webcontents and check validity.
  tab_strip_model()->CloseWebContentsAt(
      tab_strip_model()->GetIndexOfWebContents(web_contents),
      TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(tab_data->IsValidForOrganizing());
}

// TabOrganization tests.

TEST_F(TabOrganizationTest, TabOrganizationAddingTabData) {
  TabOrganization organization({}, {u"default_name"}, 0, absl::nullopt);
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 0);
  content::WebContents* web_contents = AddTab();
  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(tab_strip_model(), web_contents);

  organization.AddTabData(std::move(tab_data));
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 1);
}

TEST_F(TabOrganizationTest, TabOrganizationRemovingTabData) {
  TabOrganization organization({}, {u"default_name"}, 0, absl::nullopt);
  content::WebContents* web_contents = AddTab();
  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(tab_strip_model(), web_contents);
  TabData::TabID tab_data_id = tab_data->tab_id();
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

TEST_F(TabOrganizationTest, TabOrganizationIsValidForOrganizing) {
  TabOrganization organization({}, {u"default_name"}, 0, absl::nullopt);

  content::WebContents* tab_1 = AddTab();
  std::unique_ptr<TabData> tab_data_1 =
      std::make_unique<TabData>(tab_strip_model(), tab_1);
  organization.AddTabData(std::move(tab_data_1));

  EXPECT_FALSE(organization.IsValidForOrganizing());

  content::WebContents* tab_2 = AddTab();
  std::unique_ptr<TabData> tab_data_2 =
      std::make_unique<TabData>(tab_strip_model(), tab_2);
  TabData* tab_data_2_ptr = tab_data_2.get();
  organization.AddTabData(std::move(tab_data_2));
  EXPECT_TRUE(organization.IsValidForOrganizing());

  InvalidateTabData(tab_data_2_ptr);
  EXPECT_FALSE(organization.IsValidForOrganizing());

  content::WebContents* tab_3 = AddTab();
  std::unique_ptr<TabData> tab_data_3 =
      std::make_unique<TabData>(tab_strip_model(), tab_3);
  organization.AddTabData(std::move(tab_data_3));
  EXPECT_TRUE(organization.IsValidForOrganizing());
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

TEST_F(TabOrganizationTest,
       TabOrganizationSessionDestructionCancelsRequestIfStarted) {
  bool cancel_called = false;
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>(
          base::DoNothing(), base::BindLambdaForTesting(
                                 [&](const TabOrganizationRequest* request) {
                                   cancel_called = true;
                                 }));

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));
  session->StartRequest();
  session.reset();

  EXPECT_TRUE(cancel_called);
}
