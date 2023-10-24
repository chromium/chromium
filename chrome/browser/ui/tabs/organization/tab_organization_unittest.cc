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
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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

TEST_F(TabOrganizationTest, TabOrganizationIDs) {
  TabOrganization organization_1({}, {u"default_name"}, 0, absl::nullopt);
  TabOrganization organization_2({}, {u"default_name"}, 0, absl::nullopt);

  EXPECT_NE(organization_1.organization_id(), organization_2.organization_id());
}

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

TEST_F(TabOrganizationTest, TabOrganizationReject) {
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

// TabOrganizationRequest tests.
TEST_F(TabOrganizationTest, TabOrganizationRequestOnStartRequest) {
  bool start_called = false;
  TabOrganizationRequest request(base::BindLambdaForTesting(
      [&](const TabOrganizationRequest* request,
          TabOrganizationRequest::BackendCompletionCallback on_completion,
          TabOrganizationRequest::BackendFailureCallback on_failure) {
        start_called = true;
      }));
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

// TabOrganizationSession Tests.

TEST_F(TabOrganizationTest, TabOrganizationSessionIDs) {
  std::unique_ptr<TabOrganizationSession> session1 =
      std::make_unique<TabOrganizationSession>();
  std::unique_ptr<TabOrganizationSession> session2 =
      std::make_unique<TabOrganizationSession>();
  EXPECT_NE(session1->session_id(), session2->session_id());
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
      std::make_unique<TabOrganizationSession>(nullptr, std::move(request));
  session->StartRequest();
  session.reset();

  EXPECT_TRUE(cancel_called);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionGetNextTabOrganization) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(
          nullptr, std::make_unique<TabOrganizationRequest>());

  EXPECT_EQ(session->GetNextTabOrganization(), nullptr);

  const std::u16string name = u"default_name";
  session->AddOrganizationForTesting(
      TabOrganization({}, {name}, 0, absl::nullopt));
  const TabOrganization* const organization_1 =
      session->GetNextTabOrganization();
  EXPECT_NE(organization_1, nullptr);
  EXPECT_EQ(organization_1->GetDisplayName(), name);

  // If the organization didnt change state, the session should still return
  // the same organization.
  TabOrganization* const organization_2 = session->GetNextTabOrganization();
  EXPECT_EQ(organization_1, organization_2);
  EXPECT_EQ(organization_2->GetDisplayName(), name);
}

TEST_F(TabOrganizationTest,
       TabOrganizationSessionGetNextTabOrganizationAfterAccept) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(
          nullptr, std::make_unique<TabOrganizationRequest>());

  EXPECT_EQ(session->GetNextTabOrganization(), nullptr);

  TabOrganization organization({}, {u"Organization"}, 0, absl::nullopt);
  organization.AddTabData(
      std::make_unique<TabData>(tab_strip_model(), AddTab(tab_strip_model())));
  organization.AddTabData(
      std::make_unique<TabData>(tab_strip_model(), AddTab(tab_strip_model())));

  const std::u16string name = u"default_name";
  session->AddOrganizationForTesting(std::move(organization));
  TabOrganization* const organization_1 = session->GetNextTabOrganization();

  organization_1->Accept();
  // Now that the organization has been accepted it should not show up as the
  // next organization.
  const TabOrganization* const organization_2 =
      session->GetNextTabOrganization();
  EXPECT_EQ(organization_2, nullptr);
}

TEST_F(TabOrganizationTest,
       TabOrganizationSessionGetNextTabOrganizationAfterReject) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(
          nullptr, std::make_unique<TabOrganizationRequest>());

  EXPECT_EQ(session->GetNextTabOrganization(), nullptr);

  const std::u16string name = u"default_name";
  session->AddOrganizationForTesting(
      TabOrganization({}, {name}, 0, absl::nullopt));
  TabOrganization* const organization_1 = session->GetNextTabOrganization();
  EXPECT_NE(organization_1, nullptr);
  EXPECT_EQ(organization_1->GetDisplayName(), name);

  organization_1->Reject();
  // Now that the organization has been rejected it should not show up as the
  // next organization.
  const TabOrganization* const organization_2 =
      session->GetNextTabOrganization();
  EXPECT_EQ(organization_2, nullptr);
}

TEST_F(TabOrganizationTest,
       TabOrganizationSessionPopulateOrganizationsInvalidTabNotFilled) {
  // Create a dummy request to pass through the response.
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();

  // Create a valid tab for the organization.
  content::WebContents* valid_web_contents_1 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_1 =
      std::make_unique<TabData>(tab_strip_model(), valid_web_contents_1);
  TabData::TabID valid_tab_data_id_1 = tab_data_valid_1->tab_id();
  request->AddTabData(std::move(tab_data_valid_1));

  // Create another valid tab for the organization. (2 are needed to be a valid
  // organization)
  content::WebContents* valid_web_contents_2 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_2 =
      std::make_unique<TabData>(tab_strip_model(), valid_web_contents_2);
  TabData::TabID valid_tab_data_id_2 = tab_data_valid_2->tab_id();
  request->AddTabData(std::move(tab_data_valid_2));

  // Create an invalid tab for the organization.
  std::unique_ptr<TabData> tab_data_invalid =
      std::make_unique<TabData>(tab_strip_model(), AddTab(tab_strip_model()));
  InvalidateTabData(tab_data_invalid.get());
  TabData::TabID invalid_tab_data_id = tab_data_invalid->tab_id();
  request->AddTabData(std::move(tab_data_invalid));

  // Create the session.
  TabOrganizationRequest* request_ptr = request.get();
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(nullptr, std::move(request));

  // Create the a response that uses the invalid tab.
  std::vector<TabData::TabID> tab_ids{valid_tab_data_id_1, valid_tab_data_id_2,
                                      invalid_tab_data_id};

  std::vector<TabOrganizationResponse::Organization> organizations;
  organizations.emplace_back(u"org_with_invalid_ids", tab_ids);
  EXPECT_EQ(organizations.size(), 1u);

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(organizations);
  EXPECT_EQ(response->organizations.size(), 1u);

  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));
  EXPECT_EQ(request_ptr->response()->organizations.size(), 1u);

  EXPECT_EQ(session->tab_organizations().size(), 1u);
  EXPECT_EQ(session->tab_organizations()[0].tab_datas().size(), 2u);
  EXPECT_EQ(session->tab_organizations()[0].tab_datas()[0]->web_contents(),
            valid_web_contents_1);
  EXPECT_EQ(session->tab_organizations()[0].tab_datas()[1]->web_contents(),
            valid_web_contents_2);
}

TEST_F(TabOrganizationTest,
       TabOrganizationSessionPopulateOrganizationsMissingTabIDNotFilled) {
  // Create a dummy request to pass through the response.
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();

  // Create a valid tab for the organization.
  content::WebContents* valid_web_contents_1 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_1 =
      std::make_unique<TabData>(tab_strip_model(), valid_web_contents_1);
  TabData::TabID valid_tab_data_id_1 = tab_data_valid_1->tab_id();
  request->AddTabData(std::move(tab_data_valid_1));

  // Create another valid tab for the organization. (2 are needed to be a valid
  // organization)
  content::WebContents* valid_web_contents_2 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_2 =
      std::make_unique<TabData>(tab_strip_model(), valid_web_contents_2);
  TabData::TabID valid_tab_data_id_2 = tab_data_valid_2->tab_id();
  request->AddTabData(std::move(tab_data_valid_2));

  // Create an invalid tab for the organization.
  content::WebContents* missing_web_contents = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_missing =
      std::make_unique<TabData>(tab_strip_model(), missing_web_contents);
  TabData::TabID missing_tab_data_id = tab_data_missing->tab_id();
  request->AddTabData(std::move(tab_data_missing));

  // Destroy the webcontents.
  tab_strip_model()->CloseWebContentsAt(
      tab_strip_model()->GetIndexOfWebContents(missing_web_contents),
      TabCloseTypes::CLOSE_NONE);

  // Create the session.
  TabOrganizationRequest* request_ptr = request.get();
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(nullptr, std::move(request));

  // Create the a response that uses the missing tab.
  std::vector<TabData::TabID> tab_ids{valid_tab_data_id_1, valid_tab_data_id_2,
                                      missing_tab_data_id};

  std::vector<TabOrganizationResponse::Organization> organizations;
  organizations.emplace_back(u"org_with_missing_ids", tab_ids);
  EXPECT_EQ(organizations.size(), 1u);

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(organizations);
  EXPECT_EQ(response->organizations.size(), 1u);

  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));
  EXPECT_EQ(request_ptr->response()->organizations.size(), 1u);

  EXPECT_EQ(session->tab_organizations().size(), 1u);
  EXPECT_EQ(session->tab_organizations()[0].tab_datas().size(), 2u);
  EXPECT_EQ(session->tab_organizations()[0].tab_datas()[0]->web_contents(),
            valid_web_contents_1);
  EXPECT_EQ(session->tab_organizations()[0].tab_datas()[1]->web_contents(),
            valid_web_contents_2);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionCreation) {
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  // Add a couple tabs with different URLs.
  for (int i = 0; i < 5; i++) {
    content::WebContents* tab = AddTab();
    request->AddTabData(std::make_unique<TabData>(tab_strip_model(), tab));
  }

  // Add 2 tabs that are grouped in the response.
  content::WebContents* tab_to_group_1 = AddTab();
  TabData* tab_to_group_data_1 = request->AddTabData(
      std::make_unique<TabData>(tab_strip_model(), tab_to_group_1));

  content::WebContents* tab_to_group_2 = AddTab();
  TabData* tab_to_group_data_2 = request->AddTabData(
      std::make_unique<TabData>(tab_strip_model(), tab_to_group_2));

  content::WebContents* tab_to_not_group = AddTab();
  request->AddTabData(
      std::make_unique<TabData>(tab_strip_model(), tab_to_not_group));

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(nullptr, std::move(request));

  std::vector<TabOrganizationResponse::Organization> response_organizations;
  TabOrganizationResponse::Organization organization(
      u"title", {tab_to_group_data_1->tab_id(), tab_to_group_data_2->tab_id()});
  response_organizations.emplace_back(std::move(organization));

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(response_organizations);

  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));

  const std::vector<TabOrganization>& organizations =
      session->tab_organizations();
  EXPECT_EQ(organizations.size(), 1u);

  /*
    TODO, once completion does not call PopulateAndCreate, the organization must
    be accepted for the group to be created.

    TabOrganization* next_organization = session->GetNextTabOrganization();
    EXPECT_NE(next_organization, nullptr);
    next_organization->Accept();
  */
  EXPECT_EQ(tab_strip_model()->group_model()->ListTabGroups().size(), 1u);
  const tab_groups::TabGroupId group_id =
      tab_strip_model()->group_model()->ListTabGroups().at(0);

  absl::optional<tab_groups::TabGroupId> group_for_tab_1 =
      tab_strip_model()->GetTabGroupForTab(
          tab_strip_model()->GetIndexOfWebContents(tab_to_group_1));
  EXPECT_TRUE(group_for_tab_1.has_value());
  EXPECT_EQ(group_for_tab_1.value(), group_id);

  absl::optional<tab_groups::TabGroupId> group_for_tab_2 =
      tab_strip_model()->GetTabGroupForTab(
          tab_strip_model()->GetIndexOfWebContents(tab_to_group_2));
  EXPECT_TRUE(group_for_tab_2.has_value());
  EXPECT_EQ(group_for_tab_2.value(), group_id);

  absl::optional<tab_groups::TabGroupId> group_for_tab_to_not_group =
      tab_strip_model()->GetTabGroupForTab(
          tab_strip_model()->GetIndexOfWebContents(tab_to_not_group));
  EXPECT_FALSE(group_for_tab_to_not_group.has_value());
}
