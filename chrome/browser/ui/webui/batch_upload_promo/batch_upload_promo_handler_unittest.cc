// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/batch_upload_promo/batch_upload_promo_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_test_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/js/batch_upload_promo/batch_upload_promo.mojom.h"

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

constexpr int kBookmarksCount = 3;
constexpr int kPasswordsCount = 2;

using testing::_;

class MockPage : public batch_upload_promo::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<batch_upload_promo::mojom::Page> BindAndGetRemote() {
    CHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, OnLocalDataCountChanged, (int32_t), (override));

  mojo::Receiver<batch_upload_promo::mojom::Page> receiver_{this};
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindRepeating(&CreateTestSyncService));
  return profile_builder.Build();
}

}  // namespace

class BatchUploadPromoHandlerTest : public testing::Test {
 public:
  BatchUploadPromoHandlerTest()
      : profile_(MakeTestingProfile()),
        web_contents_(web_contents_factory_.CreateWebContents(profile_.get())) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                              syncer::kUnoPhase2FollowUp},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    test_helper_.SetupBatchUploadTestingFactoryInProfile(profile_.get());
    handler_ = std::make_unique<BatchUploadPromoHandler>(
        mojo::PendingReceiver<batch_upload_promo::mojom::PageHandler>(),
        page_.BindAndGetRemote(), profile_.get(), web_contents_);
  }

  void AddBatchUploadData() {
    test_helper_.SetReturnDescriptions(syncer::BOOKMARKS,
                                       /*item_count=*/kBookmarksCount);
    test_helper_.SetReturnDescriptions(syncer::PASSWORDS,
                                       /*item_count=*/kPasswordsCount);
  }

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  BatchUploadPromoHandler* handler() { return handler_.get(); }
  MockPage* mock_page() { return &page_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  testing::StrictMock<MockPage> page_;
  std::unique_ptr<BatchUploadPromoHandler> handler_;
  BatchUploadServiceTestHelper test_helper_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BatchUploadPromoHandlerTest, SendEmptyBatchUploadData) {
  EXPECT_CALL(*mock_page(), OnLocalDataCountChanged(0));

  test_sync_service()->FireStateChanged();
  mock_page()->FlushForTesting();
}

TEST_F(BatchUploadPromoHandlerTest, GetEmptyBatchUploadData) {
  base::MockCallback<base::OnceCallback<void(int32_t)>> callback;
  EXPECT_CALL(callback, Run(0)).Times(1);

  handler()->GetBatchUploadPromoLocalDataCount(callback.Get());
}

TEST_F(BatchUploadPromoHandlerTest, SendBatchUploadDataWithLocalItems) {
  AddBatchUploadData();
  EXPECT_CALL(*mock_page(),
              OnLocalDataCountChanged(kBookmarksCount + kPasswordsCount));

  test_sync_service()->FireStateChanged();
  mock_page()->FlushForTesting();
}

TEST_F(BatchUploadPromoHandlerTest, GetBatchUploadDataWithLocalItems) {
  AddBatchUploadData();

  base::MockCallback<base::OnceCallback<void(int32_t)>> callback;
  EXPECT_CALL(callback, Run(kBookmarksCount + kPasswordsCount)).Times(1);

  handler()->GetBatchUploadPromoLocalDataCount(callback.Get());
}

TEST_F(BatchUploadPromoHandlerTest, SendEmptyBatchUploadDataWithSyncDisabled) {
  AddBatchUploadData();
  test_sync_service()->SetAllowedByEnterprisePolicy(false);
  EXPECT_CALL(*mock_page(), OnLocalDataCountChanged(0));

  test_sync_service()->FireStateChanged();
  mock_page()->FlushForTesting();
}

TEST_F(BatchUploadPromoHandlerTest,
       SyncStateChangeDoesNotSendDataWhenConfiguring) {
  test_sync_service()->SetMaxTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  EXPECT_CALL(*mock_page(), OnLocalDataCountChanged(_)).Times(0);

  test_sync_service()->FireStateChanged();
  mock_page()->FlushForTesting();
}

TEST_F(BatchUploadPromoHandlerTest, OnBatchUploadPromoClicked) {
  // This test doesn't create a Browser instance, so `FindBrowserWithTab()`
  // returns `nullptr`. OnBatchUploadPromoClicked should handle this without
  // crashing.
  handler()->OnBatchUploadPromoClicked();
}
