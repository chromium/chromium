// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/tabs/organization/logging_util.h"
#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int kMinimumValidTabs = 2;

class FakeModelQualityLogEntry
    : public optimization_guide::ModelQualityLogEntry {
 public:
  FakeModelQualityLogEntry()
      : optimization_guide::ModelQualityLogEntry(
            std::make_unique<optimization_guide::proto::LogAiDataRequest>(),
            nullptr) {}
};

}  // anonymous namespace

class TabOrganizationTest : public testing::Test {
 public:
  struct StoredOnResponseCallback {
    bool was_called = false;

    void OnResponse(TabOrganizationResponse* response) { was_called = true; }
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
    GURL url("http://page_" + base::NumberToString(offset));
    offset++;
    return url;
  }

  tabs::TabModel* AddTab(TabStripModel* tab_strip_model = nullptr,
                         std::optional<GURL> url = std::nullopt) {
    std::unique_ptr<content::WebContents> contents_unique_ptr =
        CreateWebContents();
    content::WebContentsTester::For(contents_unique_ptr.get())
        ->NavigateAndCommit(url.has_value() ? url.value() : GetUniqueTestURL());
    content::WebContents* content_ptr = contents_unique_ptr.get();
    if (!tab_strip_model) {
      tab_strip_model = tab_strip_model_.get();
    }
    tab_strip_model->AppendWebContents(std::move(contents_unique_ptr), true);
    return tab_strip_model->GetTabForWebContents(content_ptr);
  }

  void InvalidateTabData(TabData* tab_data) {
    // TabData is invalidated from new URL which are different from their
    // original URL. as long as the original URL was created via
    // GetUniqueTestURL this will invalidate.
    content::WebContentsTester::For(tab_data->tab()->contents())
        ->NavigateAndCommit(GetUniqueTestURL());
  }

  std::unique_ptr<TabOrganization> CreateValidOrganization() {
    const std::u16string name = u"default_name";
    std::unique_ptr<TabOrganization> organization =
        std::make_unique<TabOrganization>(
            std::vector<std::unique_ptr<TabData>>{},
            std::vector<std::u16string>{name});

    organization->AddTabData(
        std::make_unique<TabData>(AddTab(tab_strip_model())));

    organization->AddTabData(
        std::make_unique<TabData>(AddTab(tab_strip_model())));

    return organization;
  }

  std::unique_ptr<TabOrganizationSession> CreateSessionWithValidOrganization(
      TabOrganizationEntryPoint entrypoint = TabOrganizationEntryPoint::kNone) {
    std::unique_ptr<TabOrganizationRequest> request =
        std::make_unique<TabOrganizationRequest>();
    TabOrganizationRequest* request_ptr = request.get();

    std::vector<TabOrganizationResponse::Organization> response_organizations;

    std::vector<TabData::TabID> ids_to_group;
    for (int i = 0; i < kMinimumValidTabs; i++) {
      TabData* tab_to_group_data =
          request->AddTabData(std::make_unique<TabData>(AddTab()));
      ids_to_group.emplace_back(tab_to_group_data->tab_id());
    }
    TabOrganizationResponse::Organization organization(u"label",
                                                       std::move(ids_to_group));
    response_organizations.emplace_back(std::move(organization));

    std::unique_ptr<TabOrganizationResponse> response =
        std::make_unique<TabOrganizationResponse>(response_organizations);

    std::unique_ptr<TabOrganizationSession> session =
        std::make_unique<TabOrganizationSession>(std::move(request),
                                                 entrypoint);

    session->StartRequest();
    request_ptr->CompleteRequestForTesting(std::move(response));
    return session;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;

  const std::unique_ptr<TestTabStripModelDelegate> delegate_;
  const std::unique_ptr<TabStripModel> tab_strip_model_;
  tabs::PreventTabFeatureInitialization prevent_;
};

class SessionObserver : public TabOrganizationSession::Observer {
 public:
  void OnTabOrganizationSessionUpdated(
      const TabOrganizationSession* session) override {
    update_call_count++;
  }

  void OnTabOrganizationSessionDestroyed(
      TabOrganizationSession::ID session_id) override {
    if (!session_) {
      return;
    }

    destroy_call_count++;
    session_ = nullptr;
  }

  ~SessionObserver() override {
    if (session_) {
      session_->RemoveObserver(this);
    }
  }

  int update_call_count = 0;
  int destroy_call_count = 0;
  raw_ptr<TabOrganizationSession> session_;
};

// TabData tests.

// The constructor that takes the webcontents and tabstrip model should
// instantiate correctly.
TEST_F(TabOrganizationTest, TabDataTabStripModelConstructor) {
  tabs::TabModel* tab = AddTab();
  TabData tab_data(tab);
  EXPECT_EQ(tab_strip_model(), tab_data.original_tab_strip_model());
  EXPECT_EQ(tab->contents()->GetLastCommittedURL(), tab_data.original_url());

  // TODO(crbug.com/40070608) Add a check for TabID once TabStripModel::Tab has
  // the new handle.
}

// Check that TabData isn't updated when the tabstrip updates.
TEST_F(TabOrganizationTest, TabDataTabStripTabUpdatingURL) {
  tabs::TabModel* tab = AddTab();
  GURL old_gurl = GURL(GetUniqueTestURL());
  content::WebContentsTester::For(tab->contents())->NavigateAndCommit(old_gurl);

  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);

  // When updating tab URL, the TabData shouldn't update.
  content::WebContentsTester::For(tab->contents())
      ->NavigateAndCommit(GURL(GetUniqueTestURL()));
  EXPECT_NE(tab_data->original_url(), tab->contents()->GetLastCommittedURL());
}

TEST_F(TabOrganizationTest, TabDataOnTabStripModelDestroyed) {
  // Create a destroyable tabstripmodel.
  std::unique_ptr<TabStripModel> new_tab_strip_model =
      std::make_unique<TabStripModel>(delegate(), profile());

  // Create a tab data that should be listening to the tabstrip model.
  std::unique_ptr<TabData> tab_data =
      std::make_unique<TabData>(AddTab(new_tab_strip_model.get()));

  // destroy the tabstripmodel. expect that the original tab strip model is
  // nullptr.
  EXPECT_EQ(tab_data->original_tab_strip_model(), new_tab_strip_model.get());
  new_tab_strip_model.reset();
  EXPECT_EQ(tab_data->original_tab_strip_model(), nullptr);
}

TEST_F(TabOrganizationTest, TabDataOnDestroyWebContentsSetToNull) {
  tabs::TabModel* tab = AddTab();

  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);

  tab_strip_model()->CloseWebContentsAt(tab_strip_model()->GetIndexOfTab(tab),
                                        TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(tab_data->tab(), nullptr);
}

TEST_F(TabOrganizationTest, TabDataOnDestroyWebContentsReplaceUpdatesContents) {
  tabs::TabModel* tab = AddTab();
  content::WebContents* old_contents = tab->contents();

  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);

  std::unique_ptr<content::WebContents> new_contents = CreateWebContents();
  content::WebContents* new_contents_ptr = new_contents.get();
  // Should initially be observing the old WebContents.
  // N.B. this is calling `WebContentsObserver::web_contents()`.
  ASSERT_EQ(tab_data->web_contents(), old_contents);

  tab_strip_model()->DiscardWebContentsAt(tab_strip_model()->GetIndexOfTab(tab),
                                          std::move(new_contents));

  // Same tab, containing a different WebContents.
  EXPECT_EQ(tab_data->tab(), tab);
  EXPECT_EQ(tab_data->tab()->contents(), new_contents_ptr);
  // Should be observing the new WebContents.
  // N.B. this is calling `WebContentsObserver::web_contents()`.
  EXPECT_EQ(tab_data->web_contents(), new_contents_ptr);
}

TEST_F(TabOrganizationTest, TabDataURLChangeIsNotValidForOrganizing) {
  tabs::TabModel* tab = AddTab();
  GURL old_gurl = GURL(GetUniqueTestURL());
  content::WebContentsTester::For(tab->contents())->NavigateAndCommit(old_gurl);

  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);

  EXPECT_TRUE(tab_data->IsValidForOrganizing());

  // update the URL for the webcontents, expect the tab data to not be valid.
  // When updating tab URL, the TabData shouldn't update.
  content::WebContentsTester::For(tab_data->tab()->contents())
      ->NavigateAndCommit(GetUniqueTestURL());
  EXPECT_FALSE(tab_data->IsValidForOrganizing());
}

TEST_F(TabOrganizationTest, TabDataWebContentsDeletionIsNotValidForOrganizing) {
  tabs::TabModel* tab = AddTab();
  GURL old_gurl = GURL(GetUniqueTestURL());
  content::WebContentsTester::For(tab->contents())->NavigateAndCommit(old_gurl);

  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);
  EXPECT_TRUE(tab_data->IsValidForOrganizing());

  // Add a new tab so that the tabstripmodel doesnt close.
  AddTab();

  // Delete the webcontents and check validity.
  tab_strip_model()->CloseWebContentsAt(tab_strip_model()->GetIndexOfTab(tab),
                                        TabCloseTypes::CLOSE_NONE);
  EXPECT_FALSE(tab_data->IsValidForOrganizing());
}

TEST_F(TabOrganizationTest, TabDataObserverTest) {
  class TestObserver : public TabData::Observer {
   public:
    void OnTabDataUpdated(const TabData* tab_data) override {
      update_call_count++;
    }

    void OnTabDataDestroyed(TabData::TabID tab_id) override {
      if (!tab_data_) {
        return;
      }

      destroy_call_count++;
      tab_data_ = nullptr;
    }

    ~TestObserver() override {
      if (tab_data_) {
        tab_data_->RemoveObserver(this);
      }
    }

    int update_call_count = 0;
    int destroy_call_count = 0;
    raw_ptr<TabData> tab_data_;
  };

  tabs::TabModel* old_tab = AddTab();
  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(old_tab);

  {
    TabData::Observer default_observer;
    tab_data->AddObserver(&default_observer);
    tab_data->RemoveObserver(&default_observer);
  }

  TestObserver observer;
  tab_data->AddObserver(&observer);
  observer.tab_data_ = tab_data.get();

  // replace the contents which should result in an update call.
  std::unique_ptr<content::WebContents> new_contents = CreateWebContents();
  content::WebContents* new_contents_ptr = new_contents.get();
  tab_strip_model()->DiscardWebContentsAt(
      tab_strip_model()->GetIndexOfTab(old_tab), std::move(new_contents));
  EXPECT_EQ(observer.update_call_count, 1);

  content::WebContentsTester::For(tab_data->tab()->contents())
      ->NavigateAndCommit(GetUniqueTestURL());
  EXPECT_EQ(observer.update_call_count, 2);

  // Add another tab so that the tabstripmodel doesnt go away. Arbitrary
  // TabStripModelChanges should not update the observer.
  AddTab();
  EXPECT_EQ(observer.update_call_count, 2);

  // Remove the original webcontents and expect an update call.
  tab_strip_model()->CloseWebContentsAt(
      tab_strip_model()->GetIndexOfWebContents(new_contents_ptr),
      TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(observer.update_call_count, 3);
}

TEST_F(TabOrganizationTest, TabDataHttpHttpsOnlyURLs) {
  {
    tabs::TabModel* tab = AddTab(tab_strip_model(), GURL("http://zombo.com"));
    TabData tab_data(tab);
    EXPECT_TRUE(tab_data.IsValidForOrganizing());
  }
  {
    tabs::TabModel* tab = AddTab(tab_strip_model(), GURL("https://zombo.com"));
    TabData tab_data(tab);
    EXPECT_TRUE(tab_data.IsValidForOrganizing());
  }
  {
    tabs::TabModel* tab = AddTab(tab_strip_model(), GURL("chrome://page"));
    TabData tab_data(tab);
    EXPECT_FALSE(tab_data.IsValidForOrganizing());
  }
  {
    tabs::TabModel* tab =
        AddTab(tab_strip_model(), GURL("file://dangerous_file.exe"));
    TabData tab_data(tab);
    EXPECT_FALSE(tab_data.IsValidForOrganizing());
  }
}

TEST_F(TabOrganizationTest, TabDataPinnedTabsNotValid) {
  tabs::TabModel* tab = AddTab();
  {
    TabData tab_data(tab);
    EXPECT_TRUE(tab_data.IsValidForOrganizing());
  }
  tab_strip_model()->SetTabPinned(tab_strip_model()->GetIndexOfTab(tab), true);
  {
    TabData tab_data(tab);
    EXPECT_FALSE(tab_data.IsValidForOrganizing());
  }
}

// TabOrganization tests.

TEST_F(TabOrganizationTest, TabOrganizationIDs) {
  TabOrganization organization_1({}, {u"default_name"});
  TabOrganization organization_2({}, {u"default_name"});

  EXPECT_NE(organization_1.organization_id(), organization_2.organization_id());
}

TEST_F(TabOrganizationTest, TabOrganizationAddingTabData) {
  TabOrganization organization({}, {u"default_name"});
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 0);
  tabs::TabModel* tab = AddTab();
  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);

  organization.AddTabData(std::move(tab_data));
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 1);
}

TEST_F(TabOrganizationTest, TabOrganizationRemovingTabData) {
  TabOrganization organization({}, {u"default_name"});
  tabs::TabModel* tab = AddTab();
  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(tab);
  TabData::TabID tab_data_id = tab_data->tab_id();
  organization.AddTabData(std::move(tab_data));
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 1);

  organization.RemoveTabData(tab_data_id);
  EXPECT_EQ(static_cast<int>(organization.tab_datas().size()), 0);
}

TEST_F(TabOrganizationTest, TabOrganizationChangingCurrentName) {
  std::u16string name_0 = u"name_0";
  std::u16string name_1 = u"name_1";
  TabOrganization organization({}, {name_0, name_1});
  EXPECT_TRUE(absl::holds_alternative<size_t>(organization.current_name()));
  EXPECT_EQ(static_cast<int>(absl::get<size_t>(organization.current_name())),
            0);
  EXPECT_EQ(organization.GetDisplayName(), name_0);

  organization.SetCurrentName(1u);
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
  TabOrganization reject_organization({}, {u"default_name"});

  reject_organization.Reject();
  EXPECT_EQ(reject_organization.choice(),
            TabOrganization::UserChoice::kRejected);
}

TEST_F(TabOrganizationTest, TabOrganizationCHECKOnChangingUserChoiceTwice) {
  TabOrganization organization({}, {u"default_name"}, /*first_new_tab_index=*/0,
                               /*current_name=*/0u,
                               TabOrganization::UserChoice::kAccepted);

  EXPECT_DEATH(organization.Reject(), "");
}

TEST_F(TabOrganizationTest, TabOrganizationIsValidForOrganizing) {
  TabOrganization organization({}, {u"default_name"});

  tabs::TabModel* tab_1 = AddTab();
  std::unique_ptr<TabData> tab_data_1 = std::make_unique<TabData>(tab_1);
  organization.AddTabData(std::move(tab_data_1));

  EXPECT_FALSE(organization.IsValidForOrganizing());

  tabs::TabModel* tab_2 = AddTab();
  std::unique_ptr<TabData> tab_data_2 = std::make_unique<TabData>(tab_2);
  TabData* tab_data_2_ptr = tab_data_2.get();
  organization.AddTabData(std::move(tab_data_2));
  EXPECT_TRUE(organization.IsValidForOrganizing());

  InvalidateTabData(tab_data_2_ptr);
  EXPECT_FALSE(organization.IsValidForOrganizing());

  // Organization has been force invalidated, so it should return false for
  // IsValidForOrganizing.
  tabs::TabModel* tab_3 = AddTab();
  std::unique_ptr<TabData> tab_data_3 = std::make_unique<TabData>(tab_3);
  organization.AddTabData(std::move(tab_data_3));
  EXPECT_FALSE(organization.IsValidForOrganizing());

  TabOrganization pre_existing_organization({}, {u"default_name"}, 2);

  tabs::TabModel* grouped_tab_1 = AddTab();
  std::unique_ptr<TabData> grouped_tab_data_1 =
      std::make_unique<TabData>(grouped_tab_1);
  pre_existing_organization.AddTabData(std::move(grouped_tab_data_1));

  tabs::TabModel* grouped_tab_2 = AddTab();
  std::unique_ptr<TabData> grouped_tab_data_2 =
      std::make_unique<TabData>(grouped_tab_2);
  pre_existing_organization.AddTabData(std::move(grouped_tab_data_2));
  // The minimum number of tabs is met, but there are no tabs new to the group.
  EXPECT_FALSE(pre_existing_organization.IsValidForOrganizing());

  tabs::TabModel* grouped_tab_3 = AddTab();
  std::unique_ptr<TabData> grouped_tab_data_3 =
      std::make_unique<TabData>(grouped_tab_3);
  pre_existing_organization.AddTabData(std::move(grouped_tab_data_3));
  // There is one tab new to the group.
  EXPECT_TRUE(pre_existing_organization.IsValidForOrganizing());
}

TEST_F(TabOrganizationTest, TabOrganizationNoUniqueTabDatas) {
  tabs::TabModel* tab_1 = AddTab();
  TabOrganization::TabDatas duplicated_tab_datas;
  duplicated_tab_datas.emplace_back(std::make_unique<TabData>(tab_1));
  duplicated_tab_datas.emplace_back(std::make_unique<TabData>(tab_1));

  TabOrganization organization(std::move(duplicated_tab_datas),
                               {u"default_name"});
  EXPECT_EQ(organization.tab_datas().size(), 1u);
}

TEST_F(TabOrganizationTest, TabOrganizationAcceptCreatesGroupToLeft) {
  // Add some tabs before the future organized tabs.
  AddTab();
  AddTab();

  ASSERT_EQ(tab_strip_model()->group_model()->ListTabGroups().size(), 0u);

  std::unique_ptr<TabOrganization> organization = CreateValidOrganization();
  organization->Accept();
  EXPECT_EQ(tab_strip_model()->group_model()->ListTabGroups().size(), 1u);
  EXPECT_TRUE(tab_strip_model()->GetTabGroupForTab(0).has_value());
}

TEST_F(TabOrganizationTest,
       TabOrganizationAcceptCreatesGroupToRightOfPinnedAndGrouped) {
  // Add some tabs before the future organized tabs.
  tabs::TabModel* pinned_tab = AddTab();
  tab_strip_model()->SetTabPinned(tab_strip_model()->GetIndexOfTab(pinned_tab),
                                  true);

  // Add another tab to group, organized groups should come after groups that
  // are to the left of any unorganized tabs.
  tabs::TabModel* grouped_tab = AddTab();
  tab_strip_model()->AddToNewGroup(
      {tab_strip_model()->GetIndexOfTab(grouped_tab)});
  std::optional<tab_groups::TabGroupId> non_organized_group_id =
      tab_strip_model()->GetTabGroupForTab(
          tab_strip_model()->GetIndexOfTab(grouped_tab));

  ASSERT_EQ(tab_strip_model()->group_model()->ListTabGroups().size(), 1u);

  std::unique_ptr<TabOrganization> organization = CreateValidOrganization();
  EXPECT_TRUE(tab_strip_model()->GetTabGroupForTab(1).has_value());
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(1).value(),
            non_organized_group_id);

  organization->Accept();
  EXPECT_EQ(tab_strip_model()->group_model()->ListTabGroups().size(), 2u);

  // by default the group should be at the start of the tabstrip. Since this is
  // the organization is the only group in the tabstrip, check that the first
  // tab is in a group.
  EXPECT_FALSE(tab_strip_model()->GetTabGroupForTab(0).has_value());
  EXPECT_TRUE(tab_strip_model()->GetTabGroupForTab(1).has_value());
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(1).value(),
            non_organized_group_id);
  EXPECT_TRUE(tab_strip_model()->GetTabGroupForTab(2).has_value());
  EXPECT_NE(tab_strip_model()->GetTabGroupForTab(2).value(),
            non_organized_group_id);
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

TEST_F(TabOrganizationTest, TabOrganizationObserverTest) {
  class TestObserver : public TabOrganization::Observer {
   public:
    void OnTabOrganizationUpdated(
        const TabOrganization* tab_organization) override {
      update_call_count++;
    }

    void OnTabOrganizationDestroyed(
        TabOrganization::ID tab_organization_id) override {
      if (!tab_organization_) {
        return;
      }

      destroy_call_count++;
      tab_organization_ = nullptr;
    }

    ~TestObserver() override {
      if (tab_organization_) {
        tab_organization_->RemoveObserver(this);
      }
    }

    int update_call_count = 0;
    int destroy_call_count = 0;
    raw_ptr<TabOrganization> tab_organization_;
  };

  // Create an organization and observe it.
  std::unique_ptr<TabOrganization> organization = CreateValidOrganization();
  TestObserver observer;
  organization->AddObserver(&observer);
  observer.tab_organization_ = organization.get();

  // AddTabData to the organization.
  tabs::TabModel* old_tab = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data = std::make_unique<TabData>(old_tab);
  organization->AddTabData(std::move(tab_data));
  EXPECT_EQ(observer.update_call_count, 1);

  // Add another tab to create a valid organization.
  organization->AddTabData(
      std::make_unique<TabData>(AddTab(tab_strip_model())));
  EXPECT_EQ(observer.update_call_count, 2);
  organization->AddTabData(
      std::make_unique<TabData>(AddTab(tab_strip_model())));
  EXPECT_EQ(observer.update_call_count, 3);
  EXPECT_TRUE(organization->IsValidForOrganizing());

  // Accept the organization, expect the update method to be called.
  organization->Accept();
  EXPECT_EQ(observer.update_call_count, 4);

  // Destroy the organization and expect onDestroy to be called.
  organization.reset();
  EXPECT_EQ(observer.destroy_call_count, 1);
}

TEST_F(TabOrganizationTest, TabOrganizationForceInvalidation) {
  std::unique_ptr<TabOrganization> organization = CreateValidOrganization();

  content::WebContentsTester::For(
      organization->tab_datas()[0]->tab()->contents())
      ->NavigateAndCommit(GetUniqueTestURL());
  EXPECT_FALSE(organization->IsValidForOrganizing());
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
      std::make_unique<TabOrganizationSession>(std::move(request));
  session->StartRequest();
  session.reset();

  EXPECT_TRUE(cancel_called);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionGetNextTabOrganization) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(
          std::make_unique<TabOrganizationRequest>());

  EXPECT_EQ(session->GetNextTabOrganization(), nullptr);

  std::unique_ptr<TabOrganization> organization = CreateValidOrganization();
  TabOrganization::ID id = organization->organization_id();
  session->AddOrganizationForTesting(std::move(organization));
  const TabOrganization* const organization_1 =
      session->GetNextTabOrganization();
  EXPECT_NE(organization_1, nullptr);
  EXPECT_EQ(organization_1->organization_id(), id);

  {
    const TabOrganizationSession* const_session = session.get();
    const TabOrganization* const organization_from_const =
        const_session->GetNextTabOrganization();
    EXPECT_NE(organization_from_const, nullptr);
    EXPECT_EQ(organization_from_const->organization_id(), id);
  }

  // If the organization didnt change state, the session should still return
  // the same organization.
  TabOrganization* const organization_2 = session->GetNextTabOrganization();
  EXPECT_EQ(organization_1, organization_2);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionInvalidOrganizations) {
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  request->StartRequest();
  request->CompleteRequestForTesting({});

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  std::unique_ptr<TabOrganization> organization_1 = CreateValidOrganization();
  TabOrganization* organization_1_ptr = organization_1.get();
  TabOrganization::ID id_1 = organization_1->organization_id();
  session->AddOrganizationForTesting(std::move(organization_1));

  std::unique_ptr<TabOrganization> organization_2 = CreateValidOrganization();
  TabOrganization* organization_2_ptr = organization_2.get();
  TabOrganization::ID id_2 = organization_2->organization_id();
  session->AddOrganizationForTesting(std::move(organization_2));

  TabOrganization* organization = session->GetNextTabOrganization();
  EXPECT_NE(organization, nullptr);
  EXPECT_EQ(organization->organization_id(), id_1);
  EXPECT_FALSE(session->IsComplete());

  InvalidateTabData(organization_1_ptr->tab_datas()[0].get());
  organization = session->GetNextTabOrganization();
  EXPECT_NE(organization, nullptr);
  EXPECT_EQ(organization->organization_id(), id_2);
  EXPECT_FALSE(session->IsComplete());

  InvalidateTabData(organization_2_ptr->tab_datas()[0].get());
  organization = session->GetNextTabOrganization();
  EXPECT_EQ(organization, nullptr);
  EXPECT_TRUE(session->IsComplete());
}

TEST_F(TabOrganizationTest,
       TabOrganizationSessionGetNextTabOrganizationAfterAccept) {
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(
          std::make_unique<TabOrganizationRequest>());

  EXPECT_EQ(session->GetNextTabOrganization(), nullptr);

  session->AddOrganizationForTesting(CreateValidOrganization());
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
          std::make_unique<TabOrganizationRequest>());

  EXPECT_EQ(session->GetNextTabOrganization(), nullptr);

  std::unique_ptr<TabOrganization> organization = CreateValidOrganization();
  TabOrganization::ID id = organization->organization_id();

  session->AddOrganizationForTesting(std::move(organization));
  TabOrganization* const organization_1 = session->GetNextTabOrganization();
  EXPECT_NE(organization_1, nullptr);
  EXPECT_EQ(organization_1->organization_id(), id);

  organization_1->Reject();
  // Now that the organization has been rejected it should not show up as the
  // next organization.
  const TabOrganization* const organization_2 =
      session->GetNextTabOrganization();
  EXPECT_EQ(organization_2, nullptr);
}

TEST_F(TabOrganizationTest,
       SessionPopulateOrganizationsFromRequestSetsOrganizationID) {
  // Create a request
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  // Add 2 tabs for organization
  std::vector<TabData::TabID> ids_to_group;

  for (int i = 0; i < kMinimumValidTabs; i++) {
    tabs::TabModel* tab_to_group = AddTab();
    TabData* tab_to_group_data =
        request->AddTabData(std::make_unique<TabData>(tab_to_group));
    ids_to_group.emplace_back(tab_to_group_data->tab_id());
  }

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  // Start and complete the request so that the organizations are populated
  session->StartRequest();

  // Create a response
  std::vector<TabOrganizationResponse::Organization> response_organizations;
  TabOrganizationResponse::Organization organization(u"title",
                                                     std::move(ids_to_group));
  response_organizations.emplace_back(std::move(organization));
  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(
          std::move(response_organizations));

  request_ptr->CompleteRequestForTesting(std::move(response));

  // Check that the organizations were populated.
  EXPECT_EQ(session->tab_organizations().size(), 1u);
  EXPECT_EQ(session->tab_organizations().size(),
            session->request()->response()->organizations.size());

  // Check that the organization_id has been populated in the response
  // organization.
  EXPECT_EQ(session->tab_organizations()[0]->organization_id(),
            session->request()->response()->organizations[0].organization_id);
}

TEST_F(TabOrganizationTest,
       TabOrganizationSessionPopulateOrganizationsInvalidTabNotFilled) {
  // Create a dummy request to pass through the response.
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();

  // Create a valid tab for the organization.
  tabs::TabModel* valid_tab_1 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_1 =
      std::make_unique<TabData>(valid_tab_1);
  TabData::TabID valid_tab_data_id_1 = tab_data_valid_1->tab_id();
  request->AddTabData(std::move(tab_data_valid_1));

  // Create another valid tab for the organization. (2 are needed to be a valid
  // organization)
  tabs::TabModel* valid_tab_2 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_2 =
      std::make_unique<TabData>(valid_tab_2);
  TabData::TabID valid_tab_data_id_2 = tab_data_valid_2->tab_id();
  request->AddTabData(std::move(tab_data_valid_2));

  // Create an invalid tab for the organization.
  std::unique_ptr<TabData> tab_data_invalid =
      std::make_unique<TabData>(AddTab(tab_strip_model()));
  InvalidateTabData(tab_data_invalid.get());
  TabData::TabID invalid_tab_data_id = tab_data_invalid->tab_id();
  request->AddTabData(std::move(tab_data_invalid));

  // Create the session.
  TabOrganizationRequest* request_ptr = request.get();
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  // Create the a response that uses the invalid tab.
  std::vector<TabData::TabID> tab_ids{valid_tab_data_id_1, valid_tab_data_id_2,
                                      invalid_tab_data_id};

  std::vector<TabOrganizationResponse::Organization> organizations;
  organizations.emplace_back(u"org_with_invalid_ids", tab_ids);
  EXPECT_EQ(organizations.size(), 1u);

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(organizations);
  EXPECT_EQ(response->organizations.size(), 1u);
  EXPECT_EQ(response->organizations[0].tab_ids.size(), 3u);

  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));
  EXPECT_EQ(request_ptr->response()->organizations.size(), 1u);

  EXPECT_EQ(session->tab_organizations().size(), 1u);
  EXPECT_EQ(session->tab_organizations()[0]->tab_datas().size(), 2u);
  EXPECT_EQ(session->tab_organizations()[0]->tab_datas()[0]->tab(),
            valid_tab_1);
  EXPECT_EQ(session->tab_organizations()[0]->tab_datas()[1]->tab(),
            valid_tab_2);
}

TEST_F(TabOrganizationTest,
       TabOrganizationSessionPopulateOrganizationsMissingTabIDNotFilled) {
  // Create a dummy request to pass through the response.
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();

  // Create a valid tab for the organization.
  tabs::TabModel* valid_tab_1 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_1 =
      std::make_unique<TabData>(valid_tab_1);
  TabData::TabID valid_tab_data_id_1 = tab_data_valid_1->tab_id();
  request->AddTabData(std::move(tab_data_valid_1));

  // Create another valid tab for the organization. (2 are needed to be a valid
  // organization)
  tabs::TabModel* valid_tab_2 = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_valid_2 =
      std::make_unique<TabData>(valid_tab_2);
  TabData::TabID valid_tab_data_id_2 = tab_data_valid_2->tab_id();
  request->AddTabData(std::move(tab_data_valid_2));

  // Create an invalid tab for the organization.
  tabs::TabModel* missing_tab = AddTab(tab_strip_model());
  std::unique_ptr<TabData> tab_data_missing =
      std::make_unique<TabData>(missing_tab);
  TabData::TabID missing_tab_data_id = tab_data_missing->tab_id();
  request->AddTabData(std::move(tab_data_missing));

  // Destroy the webcontents.
  tab_strip_model()->CloseWebContentsAt(
      tab_strip_model()->GetIndexOfTab(missing_tab), TabCloseTypes::CLOSE_NONE);

  // Create the session.
  TabOrganizationRequest* request_ptr = request.get();
  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

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
  EXPECT_EQ(session->tab_organizations()[0]->tab_datas().size(), 2u);
  EXPECT_EQ(session->tab_organizations()[0]->tab_datas()[0]->tab(),
            valid_tab_1);
  EXPECT_EQ(session->tab_organizations()[0]->tab_datas()[1]->tab(),
            valid_tab_2);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionCreation) {
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  // Add a couple tabs with different URLs.
  for (int i = 0; i < 5; i++) {
    tabs::TabModel* tab = AddTab();
    request->AddTabData(std::make_unique<TabData>(tab));
  }

  // Add 2 tabs that are grouped in the response.
  tabs::TabModel* tab_to_group_1 = AddTab();
  TabData* tab_to_group_data_1 =
      request->AddTabData(std::make_unique<TabData>(tab_to_group_1));

  tabs::TabModel* tab_to_group_2 = AddTab();
  TabData* tab_to_group_data_2 =
      request->AddTabData(std::make_unique<TabData>(tab_to_group_2));

  tabs::TabModel* tab_to_not_group = AddTab();
  request->AddTabData(std::make_unique<TabData>(tab_to_not_group));

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  std::vector<TabOrganizationResponse::Organization> response_organizations;
  TabOrganizationResponse::Organization organization(
      u"title", {tab_to_group_data_1->tab_id(), tab_to_group_data_2->tab_id()});
  response_organizations.emplace_back(std::move(organization));

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(response_organizations);

  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));

  EXPECT_EQ(session->tab_organizations().size(), 1u);

  TabOrganization* next_organization = session->GetNextTabOrganization();
  EXPECT_NE(next_organization, nullptr);
  next_organization->Accept();

  EXPECT_EQ(tab_strip_model()->group_model()->ListTabGroups().size(), 1u);
  const tab_groups::TabGroupId group_id =
      tab_strip_model()->group_model()->ListTabGroups().at(0);

  std::optional<tab_groups::TabGroupId> group_for_tab_1 =
      tab_strip_model()->GetTabGroupForTab(
          tab_strip_model()->GetIndexOfTab(tab_to_group_1));
  EXPECT_TRUE(group_for_tab_1.has_value());
  EXPECT_EQ(group_for_tab_1.value(), group_id);

  std::optional<tab_groups::TabGroupId> group_for_tab_2 =
      tab_strip_model()->GetTabGroupForTab(
          tab_strip_model()->GetIndexOfTab(tab_to_group_2));
  EXPECT_TRUE(group_for_tab_2.has_value());
  EXPECT_EQ(group_for_tab_2.value(), group_id);

  std::optional<tab_groups::TabGroupId> group_for_tab_to_not_group =
      tab_strip_model()->GetTabGroupForTab(
          tab_strip_model()->GetIndexOfTab(tab_to_not_group));
  EXPECT_FALSE(group_for_tab_to_not_group.has_value());
}

TEST_F(TabOrganizationTest, TabOrganizationSessionObserverFail) {
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  std::unique_ptr<SessionObserver> observer =
      std::make_unique<SessionObserver>();
  session->AddObserver(observer.get());
  observer->session_ = session.get();

  session->StartRequest();
  EXPECT_EQ(observer->update_call_count, 1);

  request_ptr->FailRequest();
  EXPECT_EQ(observer->update_call_count, 2);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionObserverCompleteRequest) {
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  std::unique_ptr<SessionObserver> observer =
      std::make_unique<SessionObserver>();
  session->AddObserver(observer.get());
  observer->session_ = session.get();

  session->StartRequest();
  EXPECT_EQ(observer->update_call_count, 1);

  request_ptr->CompleteRequestForTesting({});
  EXPECT_EQ(observer->update_call_count, 2);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionObserverOrganizationUpdate) {
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  // Add a couple tabs with different URLs.
  for (int i = 0; i < 5; i++) {
    tabs::TabModel* tab = AddTab();
    request->AddTabData(std::make_unique<TabData>(tab));
  }

  // Add 2 tabs that are grouped in the response.
  tabs::TabModel* tab_to_group_1 = AddTab();
  TabData* tab_to_group_data_1 =
      request->AddTabData(std::make_unique<TabData>(tab_to_group_1));

  tabs::TabModel* tab_to_group_2 = AddTab();
  TabData* tab_to_group_data_2 =
      request->AddTabData(std::make_unique<TabData>(tab_to_group_2));

  tabs::TabModel* tab_to_not_group = AddTab();
  request->AddTabData(std::make_unique<TabData>(tab_to_not_group));

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  std::unique_ptr<SessionObserver> observer =
      std::make_unique<SessionObserver>();
  session->AddObserver(observer.get());
  observer->session_ = session.get();

  std::vector<TabOrganizationResponse::Organization> response_organizations;
  TabOrganizationResponse::Organization organization(
      u"title", {tab_to_group_data_1->tab_id(), tab_to_group_data_2->tab_id()});
  response_organizations.emplace_back(std::move(organization));

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(response_organizations);

  session->StartRequest();
  EXPECT_EQ(observer->update_call_count, 1);

  request_ptr->CompleteRequestForTesting(std::move(response));
  EXPECT_EQ(observer->update_call_count, 2);
}

TEST_F(TabOrganizationTest, TabOrganizationSessionRequestOnLogResultsCalled) {
  // Create a request
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  // Add 2 tabs for organization
  std::vector<TabData::TabID> ids_to_group;
  for (int i = 0; i < kMinimumValidTabs; i++) {
    tabs::TabModel* tab_to_group = AddTab();
    TabData* tab_to_group_data =
        request->AddTabData(std::make_unique<TabData>(tab_to_group));
    ids_to_group.emplace_back(tab_to_group_data->tab_id());
  }

  // Create a response
  std::vector<TabOrganizationResponse::Organization> response_organizations;
  TabOrganizationResponse::Organization organization(u"title",
                                                     std::move(ids_to_group));
  response_organizations.emplace_back(std::move(organization));

  // Create a log response callback
  int log_callback_called_times = 0;

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(
          response_organizations, u"",
          base::BindLambdaForTesting(
              [&](const TabOrganizationSession* _session) {
                log_callback_called_times++;
              }));

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  // Start and complete the request so that the organizations are populated
  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));

  // Log Results and expect the log_results_callback to be called.
  session.reset();
  EXPECT_EQ(log_callback_called_times, 1);
}

// Logging Util

TEST_F(TabOrganizationTest, LoggingUtilAddOrganizationsToModelQuality) {
  // Create a request
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  // Add 2 tabs for organization
  std::vector<TabData::TabID> ids_to_group;
  for (int i = 0; i < kMinimumValidTabs; i++) {
    tabs::TabModel* tab_to_group = AddTab();
    TabData* tab_to_group_data =
        request->AddTabData(std::make_unique<TabData>(tab_to_group));
    ids_to_group.emplace_back(tab_to_group_data->tab_id());
  }

  // Create a response
  std::vector<TabOrganizationResponse::Organization> response_organizations;
  TabOrganizationResponse::Organization organization(u"title",
                                                     std::move(ids_to_group));
  response_organizations.emplace_back(std::move(organization));

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(response_organizations);

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  // Start and complete the request so that the organizations are populated
  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));

  std::unique_ptr<optimization_guide::ModelQualityLogEntry>
      model_quality_log_entry = std::make_unique<FakeModelQualityLogEntry>();
  optimization_guide::proto::TabOrganizationQuality* quality =
      model_quality_log_entry
          ->quality_data<optimization_guide::TabOrganizationFeatureTypeMap>();

  EXPECT_NE(quality, nullptr);
  EXPECT_NE(session.get(), nullptr);
  AddSessionDetailsToQuality(quality, session.get());

  // get the list of organizations and expect one to be present
  EXPECT_EQ(quality->organizations_size(), 1);
}

TEST_F(TabOrganizationTest, LoggingUtilAddOrganizationsToModelQualityAccepted) {
  const std::u16string kAcceptedLabelStr = u"accepted title";
  const std::u16string kRejectedLabelString = u"rejected title";
  const std::u16string kNonAcceptedLabelString = u"no choice title";
  // Create a request
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  std::vector<TabOrganizationResponse::Organization> response_organizations;

  {  // Create Accept Organization
    // Add 2 tabs for organization
    std::vector<TabData::TabID> ids_to_group;
    for (int i = 0; i < kMinimumValidTabs; i++) {
      tabs::TabModel* tab_to_group = AddTab();
      TabData* tab_to_group_data =
          request->AddTabData(std::make_unique<TabData>(tab_to_group));
      ids_to_group.emplace_back(tab_to_group_data->tab_id());
    }
    TabOrganizationResponse::Organization organization(kAcceptedLabelStr,
                                                       std::move(ids_to_group));
    response_organizations.emplace_back(std::move(organization));
  }

  {  // Create another Organization that will be rejected.
    // Add 2 tabs for organization
    std::vector<TabData::TabID> ids_to_group;
    for (int i = 0; i < kMinimumValidTabs; i++) {
      tabs::TabModel* tab_to_group = AddTab();
      TabData* tab_to_group_data =
          request->AddTabData(std::make_unique<TabData>(tab_to_group));
      ids_to_group.emplace_back(tab_to_group_data->tab_id());
    }
    TabOrganizationResponse::Organization organization(kRejectedLabelString,
                                                       std::move(ids_to_group));
    response_organizations.emplace_back(std::move(organization));
  }

  {  // Create another Organization that wont have a choice applied.
    // Add 2 tabs for organization
    std::vector<TabData::TabID> ids_to_group;
    for (int i = 0; i < kMinimumValidTabs; i++) {
      tabs::TabModel* tab_to_group = AddTab();
      TabData* tab_to_group_data =
          request->AddTabData(std::make_unique<TabData>(tab_to_group));
      ids_to_group.emplace_back(tab_to_group_data->tab_id());
    }
    TabOrganizationResponse::Organization organization(kNonAcceptedLabelString,
                                                       std::move(ids_to_group));
    response_organizations.emplace_back(std::move(organization));
  }

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(response_organizations);

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  // Start and complete the request so that the organizations are populated
  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));

  // Accept the first organization
  {
    TabOrganization* organization = session->GetNextTabOrganization();
    EXPECT_EQ(kAcceptedLabelStr, organization->GetDisplayName());
    organization->Accept();
  }

  // Reject the next organization
  {
    TabOrganization* organization = session->GetNextTabOrganization();
    EXPECT_EQ(kRejectedLabelString, organization->GetDisplayName());
    organization->Reject();
  }

  {  // Verify that the last organization has no choice set.
    TabOrganization* organization = session->GetNextTabOrganization();
    EXPECT_EQ(kNonAcceptedLabelString, organization->GetDisplayName());
  }

  std::unique_ptr<optimization_guide::ModelQualityLogEntry>
      model_quality_log_entry = std::make_unique<FakeModelQualityLogEntry>();
  optimization_guide::proto::TabOrganizationQuality* quality =
      model_quality_log_entry
          ->quality_data<optimization_guide::TabOrganizationFeatureTypeMap>();

  EXPECT_NE(quality, nullptr);
  EXPECT_NE(session.get(), nullptr);
  AddSessionDetailsToQuality(quality, session.get());

  // get the list of organizations and expect one to be present
  EXPECT_EQ(quality->organizations_size(), 3);
  {
    const optimization_guide::proto::TabOrganizationQuality_Organization&
        quality_org = quality->organizations(0);
    EXPECT_TRUE(quality_org.has_label());

    const optimization_guide::proto::TabOrganizationQuality_Organization_Label&
        quality_org_label = quality_org.label();
    EXPECT_FALSE(quality_org_label.edited());
  }
}

TEST_F(TabOrganizationTest, HistogramLogNoOrganization) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TabOrganizationRequest> request =
      std::make_unique<TabOrganizationRequest>();
  TabOrganizationRequest* request_ptr = request.get();

  std::unique_ptr<TabOrganizationResponse> response =
      std::make_unique<TabOrganizationResponse>(
          std::vector<TabOrganizationResponse::Organization>());

  std::unique_ptr<TabOrganizationSession> session =
      std::make_unique<TabOrganizationSession>(std::move(request));

  session->StartRequest();
  request_ptr->CompleteRequestForTesting(std::move(response));

  session.reset();

  histogram_tester.ExpectUniqueSample("Tab.Organization.Response.Succeeded",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Response.TabCount", 0,
                                      1);
}

TEST_F(TabOrganizationTest, HistogramLogNoChoiceOrganization) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TabOrganizationSession> session =
      CreateSessionWithValidOrganization();

  ASSERT_NE(session->GetNextTabOrganization(), nullptr);
  session.reset();

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Organization.TabRemovedCount", 0, 0);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Organization.LabelEdited", false, 0);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.AllEntrypoints.UserChoice",
      TabOrganization::UserChoice::kNoChoice, 1);
}

TEST_F(TabOrganizationTest, HistogramLogRejectOrganization) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TabOrganizationSession> session =
      CreateSessionWithValidOrganization();

  ASSERT_NE(session->GetNextTabOrganization(), nullptr);
  session->GetNextTabOrganization()->Reject();

  session.reset();

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Organization.TabRemovedCount", 0, 0);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Organization.LabelEdited", false, 0);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.AllEntrypoints.UserChoice",
      TabOrganization::UserChoice::kRejected, 1);
}

TEST_F(TabOrganizationTest, HistogramLogAcceptOrganizationNoEntrypoint) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TabOrganizationSession> session =
      CreateSessionWithValidOrganization();

  ASSERT_NE(session->GetNextTabOrganization(), nullptr);
  session->GetNextTabOrganization()->Accept();

  session.reset();

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Organization.TabRemovedCount", 0, 1);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Organization.LabelEdited", false, 1);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.AllEntrypoints.UserChoice",
      TabOrganization::UserChoice::kAccepted, 1);
}

TEST_F(TabOrganizationTest,
       HistogramLogAcceptOrganizationTabContextMenuEntryPoint) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TabOrganizationSession> session =
      CreateSessionWithValidOrganization(
          TabOrganizationEntryPoint::kTabContextMenu);

  ASSERT_NE(session->GetNextTabOrganization(), nullptr);
  session->GetNextTabOrganization()->Accept();

  session.reset();
  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.TabContextMenu.UserChoice",
      TabOrganization::UserChoice::kAccepted, 1);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.AllEntrypoints.UserChoice",
      TabOrganization::UserChoice::kAccepted, 1);
}

TEST_F(TabOrganizationTest,
       HistogramLogRejectOrganizationThreeDotMenuEntryPoint) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TabOrganizationSession> session =
      CreateSessionWithValidOrganization(
          TabOrganizationEntryPoint::kThreeDotMenu);

  ASSERT_NE(session->GetNextTabOrganization(), nullptr);
  session->GetNextTabOrganization()->Reject();

  session.reset();
  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.ThreeDotMenu.UserChoice",
      TabOrganization::UserChoice::kRejected, 1);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.AllEntrypoints.UserChoice",
      TabOrganization::UserChoice::kRejected, 1);
}

TEST_F(TabOrganizationTest,
       HistogramLogNoChoiceOrganizationProactiveEntryPoint) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TabOrganizationSession> session =
      CreateSessionWithValidOrganization(TabOrganizationEntryPoint::kProactive);
  ASSERT_NE(session->GetNextTabOrganization(), nullptr);

  session.reset();

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.UserChoice",
                                      TabOrganization::UserChoice::kNoChoice,
                                      1);

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.AllEntrypoints.UserChoice",
      TabOrganization::UserChoice::kNoChoice, 1);
}
