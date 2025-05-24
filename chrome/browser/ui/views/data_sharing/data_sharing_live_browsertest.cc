// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/sync/service/sync_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace {

// A class to wait for a tab group with specific title to open
class SavedTabGroupServiceWaiter
    : public tab_groups::TabGroupSyncService::Observer {
 public:
  explicit SavedTabGroupServiceWaiter(
      tab_groups::TabGroupSyncService* tab_group_service,
      const std::u16string title)
      : tab_group_service_(tab_group_service), title_(title) {
    tab_group_sync_service_observation_.Observe(tab_group_service_);
    bool should_wait = true;
    for (const tab_groups::SavedTabGroup& group :
         tab_group_service_->GetAllGroups()) {
      if (group.title() == title_) {
        should_wait = false;
        break;
      }
    }
    if (should_wait) {
      run_loop_.Run();
    }
  }

  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override {
    if (run_loop_.running() && group.title() == title_) {
      run_loop_.Quit();
    }
  }

 private:
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_service_observation_{this};
  base::RunLoop run_loop_;
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_service_;
  const std::u16string title_;
};

// A class to wait for a javascript statement to return true.
class EvalScriptWaiter {
 public:
  EvalScriptWaiter(content::WebContents* web_contents,
                   const std::string& script)
      : web_contents_(web_contents), script_(script) {
    if (content::EvalJs(web_contents_, script_).ExtractBool()) {
      return;
    }
    timer_.Start(FROM_HERE, base::Seconds(0.1), this,
                 &EvalScriptWaiter::EvalScript);
    run_loop_.Run();
  }

 private:
  void EvalScript() {
    if (content::EvalJs(web_contents_, script_).ExtractBool()) {
      timer_.Stop();
      run_loop_.Quit();
    }
  }

  raw_ptr<content::WebContents> web_contents_;
  const std::string script_;
  base::RunLoop run_loop_;
  base::RepeatingTimer timer_;
};

// End to end test for shared tab group UI on desktop.
class DataSharingLiveTest : public signin::test::LiveTest {
 public:
  DataSharingLiveTest() = default;
  ~DataSharingLiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
    constexpr char SYNC_URL[] =
        "https://chrome-sync.sandbox.google.com/chrome-sync/alpha";
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("sync-url",
                                                              SYNC_URL);
    LiveTest::SetUp();
    // Always disable animation for stability.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  signin::IdentityManager* identity_manager() {
    return signin::test::identity_manager(browser());
  }

  syncer::SyncService* sync_service() {
    return signin::test::sync_service(browser());
  }

  tab_groups::TabGroupSyncService* tab_group_service() {
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(
        browser()->profile());
  }

  void SignInAndTurnOnSync() {
    signin::test::SignInFunctions sign_in_functions =
        signin::test::SignInFunctions(
            base::BindLambdaForTesting(
                [this]() -> Browser* { return this->browser(); }),
            base::BindLambdaForTesting(
                [this](int index, const GURL& url,
                       ui::PageTransition transition) -> bool {
                  return this->AddTabAtIndex(index, url, transition);
                }));

    std::optional<signin::TestAccountSigninCredentials> test_account =
        GetTestAccounts()->GetAccount("DATA_SHARING_1");
    CHECK(test_account);
    sign_in_functions.SignInFromSettings(*test_account, 0);
    sign_in_functions.TurnOnSync(*test_account, 0);

    const CoreAccountInfo& primary_account =
        identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
    EXPECT_FALSE(primary_account.IsEmpty());
    EXPECT_TRUE(gaia::AreEmailsSame(test_account->user, primary_account.email));
    EXPECT_TRUE(sync_service()->IsSyncFeatureEnabled());
  }

  std::optional<tab_groups::TabGroupId> OpenTabGroupByTitle(
      tab_groups::TabGroupSyncService* tab_group_service,
      std::u16string title) {
    bool open = false;
    for (const tab_groups::SavedTabGroup& group :
         tab_group_service->GetAllGroups()) {
      if (group.title() == title) {
        tab_group_service->OpenTabGroup(
            group.saved_guid(),
            std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                browser(), tab_groups::OpeningSource::kUnknown));
        open = true;
      }
    }
    DCHECK(open);
    TabGroupModel* tab_group_model =
        browser()->tab_strip_model()->group_model();
    for (const tab_groups::TabGroupId& id : tab_group_model->ListTabGroups()) {
      const tab_groups::TabGroupVisualData* visual_data =
          tab_group_model->GetTabGroup(id)->visual_data();
      if (visual_data->title() == title) {
        return id;
      }
    }
    return std::nullopt;
  }

  void WaitForSDKToLoad() {
    content::WebContents* web_contents =
        DataSharingBubbleController::GetOrCreateForBrowser(browser())
            ->BubbleViewForTesting()
            ->get_contents_wrapper_for_testing()
            ->web_contents();
    EvalScriptWaiter script_waiter(
        web_contents,
        "!!document.querySelector('data-sharing-app') &&"
        "!!document.querySelector('data-sharing-app').shadowRoot &&"
        "document.querySelector('data-sharing-app').shadowRoot."
        "querySelectorAll('.peopleSharekitContainerComponents').length == 1;");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Open the share dialog of a unshared the tab group.
IN_PROC_BROWSER_TEST_F(DataSharingLiveTest, ShareUnsharedTabGroup) {
  SignInAndTurnOnSync();

  const std::u16string unshared_group_title = u"TEST UNSHARED GROUP";
  SavedTabGroupServiceWaiter waiter(tab_group_service(), unshared_group_title);

  std::optional<tab_groups::TabGroupId> tab_group_id =
      OpenTabGroupByTitle(tab_group_service(), unshared_group_title);
  CHECK(tab_group_id.has_value());

  data_sharing::RequestInfo request_info(tab_group_id.value(),
                                         data_sharing::FlowType::kShare);
  DataSharingBubbleController::GetOrCreateForBrowser(browser())->Show(
      request_info);

  WaitForSDKToLoad();
}

// Open the manage dialog of a shared tab group.
IN_PROC_BROWSER_TEST_F(DataSharingLiveTest, ManageSharedTabGroup) {
  SignInAndTurnOnSync();

  const std::u16string shared_group_title = u"TEST SHARED GROUP";
  SavedTabGroupServiceWaiter waiter(tab_group_service(), shared_group_title);

  std::optional<tab_groups::TabGroupId> tab_group_id =
      OpenTabGroupByTitle(tab_group_service(), shared_group_title);
  CHECK(tab_group_id.has_value());

  // Share the group.
  data_sharing::RequestInfo request_info(tab_group_id.value(),
                                         data_sharing::FlowType::kShare);
  DataSharingBubbleController::GetOrCreateForBrowser(browser())->Show(
      request_info);

  // Manage the group.
  request_info.type = data_sharing::FlowType::kManage;
  DataSharingBubbleController::GetOrCreateForBrowser(browser())->Show(
      request_info);

  WaitForSDKToLoad();
}

// TODO(crbug.com/366058571) Add e2e test for join flow.

}  // namespace
