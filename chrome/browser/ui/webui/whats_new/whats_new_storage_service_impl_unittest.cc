// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_storage_service_impl.h"

#include "chrome/common/chrome_version.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

class WhatsNewStorageServiceTest : public testing::Test {
 public:
  WhatsNewStorageServiceTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~WhatsNewStorageServiceTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    storage_service_ =
        std::make_unique<whats_new::WhatsNewStorageServiceImpl>();
  }

  void TearDown() override {
    storage_service_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::unique_ptr<whats_new::WhatsNewStorageService> storage_service_;
  ScopedTestingLocalState local_state_;
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

  storage_service_->ClearModule("ModuleA");
  EXPECT_EQ(static_cast<size_t>(1), storage_service_->ReadModuleData().size());
  EXPECT_EQ("ModuleB", storage_service_->ReadModuleData()[0]);

  storage_service_->ClearModule("ModuleC");
  EXPECT_EQ(static_cast<size_t>(1), storage_service_->ReadModuleData().size());

  storage_service_->ClearModule("ModuleB");
  EXPECT_EQ(static_cast<size_t>(0), storage_service_->ReadModuleData().size());
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

  storage_service_->ClearEdition("EditionA");
  EXPECT_TRUE(storage_service_->ReadEditionData().empty());
  EXPECT_EQ(static_cast<size_t>(0), storage_service_->ReadEditionData().size());
  EXPECT_EQ(nullptr, storage_service_->ReadEditionData().Find("EditionA"));
}

TEST_F(WhatsNewStorageServiceTest, StoresEditionsDataWithPreviousData) {
  ScopedDictPrefUpdate update(local_state_.Get(), prefs::kWhatsNewEditionUsed);
  update->Set("OldEdition100", 100);
  update->Set("OldEdition101", 101);
  EXPECT_FALSE(storage_service_->ReadEditionData().empty());
  EXPECT_EQ(static_cast<size_t>(2), storage_service_->ReadEditionData().size());

  storage_service_->SetEditionUsed("EditionA");
  EXPECT_EQ(static_cast<size_t>(3), storage_service_->ReadEditionData().size());
  EXPECT_EQ(CHROME_VERSION_MAJOR,
            *storage_service_->ReadEditionData().Find("EditionA"));

  storage_service_->ClearEdition("EditionA");
  EXPECT_EQ(static_cast<size_t>(2), storage_service_->ReadEditionData().size());
  EXPECT_EQ(nullptr, storage_service_->ReadEditionData().Find("EditionA"));

  storage_service_->Reset();
  EXPECT_TRUE(storage_service_->ReadEditionData().empty());
}
