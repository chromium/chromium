// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/sync_data_type_configuration.h"
#include "components/saved_tab_groups/tab_group_sync_coordinator_impl.h"
#include "components/saved_tab_groups/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/types.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace tab_groups {

class TabGroupSyncDelegateBrowserTest : public InProcessBrowserTest,
                                        public TabGroupSyncService::Observer {
 public:
  TabGroupSyncDelegateBrowserTest() {
    features_.InitWithFeatures(
        {tab_groups::kTabGroupsSaveV2,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
  }

  void OnWillBeDestroyed() override {
    if (service_) {
      service_->RemoveObserver(this);
    }

    service_ = nullptr;
    model_ = nullptr;
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&TabGroupSyncDelegateBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    TabGroupSyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindOnce(
            &TabGroupSyncDelegateBrowserTest::CreateMockTabGroupSyncService,
            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMockTabGroupSyncService(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    auto model = std::make_unique<SavedTabGroupModel>();
    model_ = model.get();

    auto saved_config = std::make_unique<SyncDataTypeConfiguration>(
        std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
            syncer::SAVED_TAB_GROUP,
            base::BindRepeating(&syncer::ReportUnrecoverableError,
                                chrome::GetChannel())),
        DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());

    syncer::DeviceInfoTracker* device_info_tracker =
        DeviceInfoSyncServiceFactory::GetForProfile(profile)
            ->GetDeviceInfoTracker();
    auto metrics_logger =
        std::make_unique<TabGroupSyncMetricsLogger>(device_info_tracker);

    auto service = std::make_unique<TabGroupSyncServiceImpl>(
        std::move(model), std::move(saved_config), nullptr, profile->GetPrefs(),
        std::move(metrics_logger));

    std::unique_ptr<TabGroupSyncDelegateDesktop> delegate =
        std::make_unique<TabGroupSyncDelegateDesktop>(service.get(), profile);

    std::unique_ptr<TabGroupSyncCoordinatorImpl> coordinator =
        std::make_unique<TabGroupSyncCoordinatorImpl>(std::move(delegate),
                                                      service.get());

    service->SetCoordinator(std::move(coordinator));
    service->SetIsInitializedForTesting(true);
    service_ = service.get();
    return std::move(service);
  }

  base::test::ScopedFeatureList features_;
  base::CallbackListSubscription subscription_;
  raw_ptr<SavedTabGroupModel> model_;
  raw_ptr<TabGroupSyncServiceImpl> service_;
};

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       RemovedGroupFromSyncClosedLocallyIfOpen) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  chrome::AddTabAt(browser(), GURL("chrome://newtab"), 0, false);

  LocalTabGroupID local_id = browser()->tab_strip_model()->AddToNewGroup({0});

  EXPECT_TRUE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(local_id));
  EXPECT_TRUE(service->GetGroup(local_id));

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->RemovedFromSync(local_id);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !browser()->tab_strip_model()->group_model()->ContainsTabGroup(
        local_id);
  }));

  EXPECT_FALSE(service->GetGroup(local_id));
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncDelegateBrowserTest,
                       AddedGroupFromSyncNotOpenedAutomatically) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddObserver(this);

  SavedTabGroup group(u"Title", tab_groups::TabGroupColorId::kBlue, {}, 0);
  const base::Uuid sync_id = group.saved_guid();
  EXPECT_FALSE(service->GetGroup(sync_id));

  // FromSync calls are asynchronous, so wait for the task to complete.
  model_->AddedFromSync(std::move(group));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service->GetGroup(sync_id).has_value(); }));

  EXPECT_EQ(
      0u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());
  EXPECT_FALSE(service->GetGroup(sync_id)->local_group_id().has_value());
}

}  // namespace tab_groups
