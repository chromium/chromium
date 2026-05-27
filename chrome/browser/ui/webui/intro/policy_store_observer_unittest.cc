// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/policy_store_observer.h"

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class FakeMachineLevelUserCloudPolicyStore
    : public policy::MachineLevelUserCloudPolicyStore {
 public:
  FakeMachineLevelUserCloudPolicyStore()
      : policy::MachineLevelUserCloudPolicyStore(
            policy::DMToken::CreateValidToken("dummy-token"),
            "dummy-client-id",
            base::FilePath(),
            base::FilePath(),
            base::FilePath(),
            base::FilePath(),
            policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
            /*background_task_runner=*/nullptr) {}

  ~FakeMachineLevelUserCloudPolicyStore() override = default;

  // Elevate the protected observer trigger methods in the base class to public
  // so that they can be called directly inside our test bodies.
  using policy::CloudPolicyStore::NotifyStoreError;
  using policy::CloudPolicyStore::NotifyStoreLoaded;
};

class PolicyStoreObserverTest : public testing::Test {
 public:
  PolicyStoreObserverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~PolicyStoreObserverTest() override = default;

  void SetUp() override {
    auto store = std::make_unique<FakeMachineLevelUserCloudPolicyStore>();
    store_ptr_ = store.get();

    manager_ = std::make_unique<policy::MachineLevelUserCloudPolicyManager>(
        std::move(store),
        /*extension_install_store=*/nullptr,
        /*external_data_manager=*/nullptr,
        /*policy_dir=*/base::FilePath(),
        scoped_refptr<base::SequencedTaskRunner>(),
        network::NetworkConnectionTrackerGetter());

    g_browser_process->browser_policy_connector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(manager_.get());
  }

  void TearDown() override {
    g_browser_process->browser_policy_connector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(nullptr);
    store_ptr_ = nullptr;
  }

  FakeMachineLevelUserCloudPolicyStore& store() {
    return CHECK_DEREF(store_ptr_);
  }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<FakeMachineLevelUserCloudPolicyStore> store_ptr_;
  std::unique_ptr<policy::MachineLevelUserCloudPolicyManager> manager_;
};

TEST_F(PolicyStoreObserverTest, StoreAlreadyInitialized) {
  store().NotifyStoreLoaded();
  ASSERT_TRUE(store().is_initialized());

  base::test::TestFuture<std::string> future;
  PolicyStoreObserver observer(future.GetCallback());

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(PolicyStoreObserverTest, StoreLoadedAsynchronously) {
  base::test::TestFuture<std::string> future;
  PolicyStoreObserver observer(future.GetCallback());

  // The store has not been loaded yet, so the callback shouldn't be called yet.
  EXPECT_FALSE(future.IsReady());

  store().NotifyStoreLoaded();

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(PolicyStoreObserverTest, StoreErrorAsynchronously) {
  base::test::TestFuture<std::string> future;
  PolicyStoreObserver observer(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  store().NotifyStoreError();

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(PolicyStoreObserverTest, StoreLoadTimeout) {
  base::test::TestFuture<std::string> future;
  PolicyStoreObserver observer(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Fast forward by 2.5 seconds, which is the timeout duration.
  task_environment().FastForwardBy(base::Seconds(2.5));

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(PolicyStoreObserverTest, StoreNull) {
  // Detach the manager so that the policy store is null.
  g_browser_process->browser_policy_connector()
      ->SetMachineLevelUserCloudPolicyManagerForTesting(nullptr);

  base::test::TestFuture<std::string> future;
  PolicyStoreObserver observer(future.GetCallback());

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(PolicyStoreObserverTest, StoreLoadedWithDeviceManager) {
  ScopedDeviceManagerForTesting scoped_manager("google.com");

  base::test::TestFuture<std::string> future;
  PolicyStoreObserver observer(future.GetCallback());

  store().NotifyStoreLoaded();

  EXPECT_EQ(future.Get(), l10n_util::GetStringFUTF8(
                              IDS_FRE_MANAGED_BY_DESCRIPTION, u"google.com"));
}

TEST_F(PolicyStoreObserverTest, StoreLoadedWithEmptyDeviceManager) {
  ScopedDeviceManagerForTesting scoped_manager("");

  base::test::TestFuture<std::string> future;
  PolicyStoreObserver observer(future.GetCallback());

  store().NotifyStoreLoaded();

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

}  // namespace
