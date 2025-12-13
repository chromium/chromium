// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"

#include "chrome/browser/global_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

class WhatsNewStorageServiceTest : public testing::Test {
 public:
  WhatsNewStorageServiceTest() = default;
  ~WhatsNewStorageServiceTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    // WhatsNewStorageServiceImpl is created and initialized in
    // GlobalFeatures::CreateWhatsNewRegistry() in the same way as the
    // production.
    storage_service_ = TestingBrowserProcess::GetGlobal()
                           ->GetFeatures()
                           ->whats_new_registry()
                           ->GetMutableStorageServiceForTesting();
    // Resets it here to satisfy the precondition.
    storage_service_->Reset();
  }

  void TearDown() override {
    storage_service_ = nullptr;
    testing::Test::TearDown();
  }

 protected:
  raw_ptr<whats_new::WhatsNewStorageService> storage_service_;
};

TEST_F(WhatsNewStorageServiceTest, StoresModulesData) {
  EXPECT_TRUE(storage_service_->ReadModuleData().empty());

  storage_service_->SetModuleEnabled("ModuleA");
  EXPECT_FALSE(storage_service_->ReadModuleData().empty());
  EXPECT_EQ(static_cast<size_t>(1), storage_service_->ReadModuleData().size());
  EXPECT_EQ("ModuleA", storage_service_->ReadModuleData().front());
  EXPECT_EQ(0, storage_service_->GetModuleQueuePosition("ModuleA"));

  storage_service_->SetModuleEnabled("ModuleB");
  EXPECT_EQ(static_cast<size_t>(2), storage_service_->ReadModuleData().size());
  EXPECT_EQ("ModuleB", storage_service_->ReadModuleData()[1]);
  EXPECT_EQ(1, storage_service_->GetModuleQueuePosition("ModuleB"));

  // Modules does not exist.
  EXPECT_EQ(-1, storage_service_->GetModuleQueuePosition("ModuleC"));

  storage_service_->ClearModules({"ModuleA"});
  EXPECT_EQ(static_cast<size_t>(1), storage_service_->ReadModuleData().size());
  EXPECT_EQ("ModuleB", storage_service_->ReadModuleData()[0]);

  storage_service_->ClearModules({"ModuleC"});
  EXPECT_EQ(static_cast<size_t>(1), storage_service_->ReadModuleData().size());

  storage_service_->ClearModules({"ModuleB"});
  EXPECT_EQ(static_cast<size_t>(0), storage_service_->ReadModuleData().size());
  EXPECT_TRUE(storage_service_->ReadModuleData().empty());
}

TEST_F(WhatsNewStorageServiceTest, ClearsMultipleModulesData) {
  EXPECT_TRUE(storage_service_->ReadModuleData().empty());

  storage_service_->SetModuleEnabled("ModuleA");
  storage_service_->SetModuleEnabled("ModuleB");
  storage_service_->SetModuleEnabled("ModuleC");
  EXPECT_EQ(static_cast<size_t>(3), storage_service_->ReadModuleData().size());

  storage_service_->ClearModules({"ModuleA", "ModuleB"});
  EXPECT_EQ(static_cast<size_t>(1), storage_service_->ReadModuleData().size());

  storage_service_->SetModuleEnabled("ModuleD");
  storage_service_->SetModuleEnabled("ModuleE");
  storage_service_->SetModuleEnabled("ModuleF");
  EXPECT_EQ(static_cast<size_t>(4), storage_service_->ReadModuleData().size());

  storage_service_->ClearModules({"ModuleC", "ModuleD", "ModuleE", "ModuleF"});
  EXPECT_TRUE(storage_service_->ReadModuleData().empty());
}

TEST_F(WhatsNewStorageServiceTest, ResetsModulesData) {
  EXPECT_TRUE(storage_service_->ReadModuleData().empty());

  storage_service_->SetModuleEnabled("ModuleA");
  storage_service_->SetModuleEnabled("ModuleB");
  storage_service_->SetModuleEnabled("ModuleC");
  EXPECT_FALSE(storage_service_->ReadModuleData().empty());
  EXPECT_EQ(static_cast<size_t>(3), storage_service_->ReadModuleData().size());

  storage_service_->Reset();
  EXPECT_TRUE(storage_service_->ReadModuleData().empty());
}

TEST_F(WhatsNewStorageServiceTest, StoresEditionsData) {
  EXPECT_TRUE(storage_service_->ReadEditionData().empty());

  storage_service_->SetEditionUsed("EditionA");
  EXPECT_FALSE(storage_service_->ReadEditionData().empty());
  EXPECT_EQ(static_cast<size_t>(1), storage_service_->ReadEditionData().size());
  EXPECT_EQ(CHROME_VERSION_MAJOR,
            *storage_service_->ReadEditionData().Find("EditionA"));
  EXPECT_EQ("EditionA", *storage_service_->FindEditionForCurrentVersion());

  // Correct editions are marked a used.
  EXPECT_TRUE(storage_service_->IsUsedEdition("EditionA"));
  EXPECT_FALSE(storage_service_->IsUsedEdition("UnusedEditionC"));

  storage_service_->ClearEditions({"EditionA"});
  EXPECT_TRUE(storage_service_->ReadEditionData().empty());
  EXPECT_EQ(static_cast<size_t>(0), storage_service_->ReadEditionData().size());
  EXPECT_EQ(nullptr, storage_service_->ReadEditionData().Find("EditionA"));
}

TEST_F(WhatsNewStorageServiceTest, StoresEditionsDataWithPreviousData) {
  ScopedDictPrefUpdate update(TestingBrowserProcess::GetGlobal()->local_state(),
                              prefs::kWhatsNewEditionUsed);
  update->Set("OldEdition100", 100);
  update->Set("OldEdition101", 101);
  EXPECT_FALSE(storage_service_->ReadEditionData().empty());
  EXPECT_EQ(static_cast<size_t>(2), storage_service_->ReadEditionData().size());

  storage_service_->SetEditionUsed("EditionA");
  EXPECT_EQ(static_cast<size_t>(3), storage_service_->ReadEditionData().size());
  EXPECT_EQ(CHROME_VERSION_MAJOR,
            *storage_service_->ReadEditionData().Find("EditionA"));

  storage_service_->ClearEditions({"EditionA"});
  EXPECT_EQ(static_cast<size_t>(2), storage_service_->ReadEditionData().size());
  EXPECT_EQ(nullptr, storage_service_->ReadEditionData().Find("EditionA"));

  storage_service_->Reset();
  EXPECT_TRUE(storage_service_->ReadEditionData().empty());
}
