// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/download_ui_adapter.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_pages/core/background/offliner_stub.h"
#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/downloads/offline_item_conversions.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "components/offline_pages/core/visuals_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

using GetVisualsOptions =
    offline_items_collection::OfflineContentProvider::GetVisualsOptions;
using offline_items_collection::OfflineItemState;

namespace offline_pages {
namespace {
using testing::_;
using testing::WithArg;

// Constants for a test OfflinePageItem.
constexpr int kTestOfflineId1 = 1;
constexpr int kTestOfflineId2 = 2;
constexpr char kTestUrl[] = "http://foo.com/bar.mhtml";
constexpr char kTestGuid1[] = "cccccccc-cccc-4ccc-0ccc-ccccccccccc1";
constexpr char kTestGuid2[] = "cccccccc-cccc-4ccc-0ccc-ccccccccccc2";
constexpr base::FilePath::CharType kTestFilePath[] =
    FILE_PATH_LITERAL("foo/bar.mhtml");
constexpr int kFileSize = 1000;
const base::Time kTestCreationTime = base::Time::Now();
constexpr char16_t kTestTitle[] = u"test title";

void GetItemAndVerify(const std::optional<OfflineItem>& expected,
                      const std::optional<OfflineItem>& actual) {
  EXPECT_EQ(expected.has_value(), actual.has_value());
  if (!expected.has_value() || !actual.has_value())
    return;

  EXPECT_EQ(expected.value().id, actual.value().id);
  EXPECT_EQ(expected.value().state, actual.value().state);
}

// Mock DownloadUIAdapter::Delegate
class DownloadUIAdapterDelegate : public DownloadUIAdapter::Delegate {
 public:
  DownloadUIAdapterDelegate() = default;
  ~DownloadUIAdapterDelegate() override = default;

  // DownloadUIAdapter::Delegate
  bool IsVisibleInUI(const ClientId& client_id) override { return is_visible; }
  void SetUIAdapter(DownloadUIAdapter* ui_adapter) override {}
  void OpenItem(const OfflineItem& item,
                int64_t offline_id,
                const OpenParams& open_params) override {}
  MOCK_METHOD2(GetShareInfoForItem,
               void(const ContentId&, OfflineContentProvider::ShareCallback));

  bool is_visible = true;
};

class MockVisualsDecoder : public VisualsDecoder {
 public:
  MOCK_METHOD2(DecodeAndCropImage_,
               void(const std::string& image_data,
                    DecodeComplete* complete_callback));
  void DecodeAndCropImage(const std::string& image_data,
                          DecodeComplete complete_callback) override {
    DecodeAndCropImage_(image_data, &complete_callback);
  }
};

// Mock OfflinePageModel for testing the SavePage calls.
class MockOfflinePageModel : public StubOfflinePageModel {
 public:
  explicit MockOfflinePageModel(base::TestMockTimeTaskRunner* task_runner)
      : observer_(nullptr), task_runner_(task_runner) {}

  MockOfflinePageModel(const MockOfflinePageModel&) = delete;
  MockOfflinePageModel& operator=(const MockOfflinePageModel&) = delete;

  ~MockOfflinePageModel() override = default;

  void AddInitialPage(ClientId client_id) {
    OfflinePageItem page(GURL(kTestUrl), kTestOfflineId1, client_id,
                         base::FilePath(kTestFilePath), kFileSize,
                         kTestCreationTime);
    page.title = kTestTitle;
    pages[kTestOfflineId1] = page;
  }

  // OfflinePageModel overrides.
  void AddObserver(Observer* observer) override {
    EXPECT_TRUE(observer != nullptr);
    observer_ = observer;
  }

  void RemoveObserver(Observer* observer) override {
    EXPECT_TRUE(observer != nullptr);
    EXPECT_EQ(observer, observer_);
    observer_ = nullptr;
  }

  // PostTask instead of just running callback to simulate the real class.
  void GetAllPages(MultipleOfflinePageItemCallback callback) override {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MockOfflinePageModel::GetAllPagesImpl,
                                  base::Unretained(this), std::move(callback)));
  }

  void GetAllPagesImpl(MultipleOfflinePageItemCallback callback) {
    std::vector<OfflinePageItem> result;
    for (const auto& page : pages)
      result.push_back(page.second);
    std::move(callback).Run(result);
  }

  void DeletePageAndNotifyAdapter(const std::string& guid) {
    for (const auto& page : pages) {
      if (page.second.client_id.id == guid) {
        observer_->OfflinePageDeleted(page.second);
        pages.erase(page.first);
        return;
      }
    }
  }

  void GetVisualsByOfflineId(
      int64_t offline_id,
      base::OnceCallback<void(std::unique_ptr<OfflinePageVisuals>)> callback)
      override {
    EXPECT_EQ(kTestOfflineId1, offline_id);

    std::unique_ptr<OfflinePageVisuals> copy;
    if (visuals_by_offline_id_result) {
      copy =
          std::make_unique<OfflinePageVisuals>(*visuals_by_offline_id_result);
    }
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(copy)));
  }

  void GetPageByOfflineId(int64_t offline_id,
                          SingleOfflinePageItemCallback callback) override {
    for (const auto& page : pages) {
      if (page.second.offline_id == offline_id) {
        std::move(callback).Run(&(page.second));
        return;
      }
    }
    std::move(callback).Run(nullptr);
  }

  void GetPagesWithCriteria(const PageCriteria& criteria,
                            MultipleOfflinePageItemCallback callback) override {
    std::vector<OfflinePageItem> matches;
    for (const auto& page : pages) {
      if (MeetsCriteria(criteria, page.second)) {
        matches.push_back(page.second);
      }
    }
    std::move(callback).Run(matches);
  }

  void AddPageAndNotifyAdapter(const OfflinePageItem& page) {
    EXPECT_EQ(pages.end(), pages.find(page.offline_id));
    pages[page.offline_id] = page;
    observer_->OfflinePageAdded(this, page);
  }

  std::map<int64_t, OfflinePageItem> pages;
  std::unique_ptr<OfflinePageVisuals> visuals_by_offline_id_result;

 private:
  raw_ptr<OfflinePageModel::Observer> observer_;
  raw_ptr<base::TestMockTimeTaskRunner> task_runner_;
};

// Creates mock versions for OfflinePageModel, RequestCoordinator and their
// dependencies, then passes them to DownloadUIAdapter for testing.
// Note that initially the OfflinePageModel is not "loaded". PumpLoop() will
// load it, firing ItemsLoaded callback to the Adapter. Hence some tests
// start from PumpLoop() right away if they don't need to test this.
class DownloadUIAdapterTest : public testing::Test,
                              public OfflineContentProvider::Observer {
 public:
  const std::string kThumbnailData = "Thumbnail-data";
  const std::string kFaviconData = "Favicon-data";
  const OfflinePageVisuals kVisuals = {kTestOfflineId1, kTestCreationTime,
                                       kThumbnailData, kFaviconData};

  DownloadUIAdapterTest();
  ~DownloadUIAdapterTest() override;

  // testing::Test
  void SetUp() override;

  // DownloadUIAdapter::Observer
  void OnItemsAdded(const std::vector<OfflineItem>& items) override;
  void OnItemUpdated(const OfflineItem& item,
                     const std::optional<UpdateDelta>& update_delta) override;
  void OnItemRemoved(const ContentId& id) override;
  void OnContentProviderGoingDown() override;

  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();

  int64_t AddRequest(const GURL& url, const ClientId& client_id);

  RequestCoordinator* request_coordinator() {
    return request_coordinator_taco_->request_coordinator();
  }
  void AddInitialPage();
  int64_t AddInitialRequest(const GURL& url, const ClientId& client_id);

  const ClientId test_client_id_other_namespace_{kLastNNamespace, kTestGuid1};
  const ClientId test_client_id1_{kAsyncNamespace, kTestGuid1};
  const ClientId test_client_id2_{kAsyncNamespace, kTestGuid2};
  const ContentId test_content_id1_{kOfflinePageNamespace, kTestGuid1};

  std::vector<std::string> added_guids, updated_guids, deleted_guids;
  int64_t download_progress_bytes;
  std::unique_ptr<MockOfflinePageModel> model;
  raw_ptr<DownloadUIAdapterDelegate> adapter_delegate;
  std::unique_ptr<DownloadUIAdapter> adapter;
  raw_ptr<OfflinerStub> offliner_stub;
  raw_ptr<MockVisualsDecoder> visuals_decoder;

 private:
  std::unique_ptr<RequestCoordinatorStubTaco> request_coordinator_taco_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

DownloadUIAdapterTest::DownloadUIAdapterTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_current_default_handle_(task_runner_) {}

DownloadUIAdapterTest::~DownloadUIAdapterTest() = default;

void DownloadUIAdapterTest::SetUp() {
  model = std::make_unique<MockOfflinePageModel>(task_runner_.get());
  std::unique_ptr<DownloadUIAdapterDelegate> delegate =
      std::make_unique<DownloadUIAdapterDelegate>();
  adapter_delegate = delegate.get();
  request_coordinator_taco_ = std::make_unique<RequestCoordinatorStubTaco>();

  std::unique_ptr<OfflinerStub> offliner = std::make_unique<OfflinerStub>();
  offliner_stub = offliner.get();
  request_coordinator_taco_->SetOffliner(std::move(offliner));

  request_coordinator_taco_->CreateRequestCoordinator();

  auto decoder = std::make_unique<MockVisualsDecoder>();
  visuals_decoder = decoder.get();

  adapter = std::make_unique<DownloadUIAdapter>(
      nullptr, model.get(), request_coordinator_taco_->request_coordinator(),
      std::move(decoder), std::move(delegate));

  adapter->AddObserver(this);
}

void DownloadUIAdapterTest::OnItemsAdded(
    const std::vector<OfflineItem>& items) {
  for (const OfflineItem& item : items) {
    added_guids.push_back(item.id.id);
  }
}

void DownloadUIAdapterTest::OnItemUpdated(
    const OfflineItem& item,
    const std::optional<UpdateDelta>& update_delta) {
  updated_guids.push_back(item.id.id);
  download_progress_bytes += item.received_bytes;
}

void DownloadUIAdapterTest::OnItemRemoved(const ContentId& id) {
  deleted_guids.push_back(id.id);
}

void DownloadUIAdapterTest::OnContentProviderGoingDown() {}

void DownloadUIAdapterTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void SavePageLaterCallback(AddRequestResult ignored) {}

int64_t DownloadUIAdapterTest::AddRequest(const GURL& url,
                                          const ClientId& client_id) {
  RequestCoordinator::SavePageLaterParams params;
  params.url = url;
  params.client_id = client_id;
  return request_coordinator()->SavePageLater(
      params, base::BindOnce(&SavePageLaterCallback));
}

void DownloadUIAdapterTest::AddInitialPage() {
  model->AddInitialPage(test_client_id1_);
  PumpLoop();
}

int64_t DownloadUIAdapterTest::AddInitialRequest(const GURL& url,
                                                 const ClientId& client_id) {
  int64_t id = AddRequest(url, client_id);
  PumpLoop();
  return id;
}

TEST_F(DownloadUIAdapterTest, InitialItemConversion) {
  AddInitialPage();
  EXPECT_EQ(1UL, model->pages.size());
  EXPECT_EQ(kTestGuid1, model->pages[kTestOfflineId1].client_id.id);

  bool called = false;
  auto callback =
      base::BindLambdaForTesting([&](const std::optional<OfflineItem>& item) {
        EXPECT_EQ(kTestGuid1, item.value().id.id);
        EXPECT_EQ(kTestUrl, item.value().url.spec());
        EXPECT_EQ(OfflineItemState::COMPLETE, item.value().state);
        EXPECT_EQ(kFileSize, item.value().received_bytes);
        EXPECT_EQ(base::FilePath(kTestFilePath), item.value().file_path);
        EXPECT_EQ(kTestCreationTime, item.value().creation_time);
        EXPECT_EQ(kFileSize, item.value().total_size_bytes);
        EXPECT_EQ(kTestTitle, base::ASCIIToUTF16(item.value().title));
        called = true;
      });

  adapter->GetItemById(test_content_id1_, callback);
  PumpLoop();
  EXPECT_TRUE(called);
}

TEST_F(DownloadUIAdapterTest, ItemDeletedAdded) {
  AddInitialPage();
  // Add page, notify adapter.
  OfflinePageItem page(GURL(kTestUrl), kTestOfflineId2, test_client_id2_,
                       base::FilePath(kTestFilePath), kFileSize,
                       kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();
  ASSERT_EQ(1UL, updated_guids.size());
  EXPECT_EQ(kTestGuid2, updated_guids[0]);
  // Remove pages, notify adapter.
  model->DeletePageAndNotifyAdapter(kTestGuid1);
  model->DeletePageAndNotifyAdapter(kTestGuid2);
  PumpLoop();
  EXPECT_EQ(2UL, deleted_guids.size());
  EXPECT_EQ(kTestGuid1, deleted_guids[0]);
  EXPECT_EQ(kTestGuid2, deleted_guids[1]);
}

TEST_F(DownloadUIAdapterTest, NotVisibleItem) {
  AddInitialPage();
  adapter_delegate->is_visible = false;
  OfflinePageItem page1(
      GURL(kTestUrl), kTestOfflineId2, test_client_id_other_namespace_,
      base::FilePath(kTestFilePath), kFileSize, kTestCreationTime);
  model->AddPageAndNotifyAdapter(page1);
  PumpLoop();
  // Should not add the page.
  EXPECT_EQ(0UL, added_guids.size());
}

TEST_F(DownloadUIAdapterTest, PageInvisibleOnUIAdded) {
  // Add a new page which should not be shown in UI.
  adapter_delegate->is_visible = false;
  OfflinePageItem page(
      GURL(kTestUrl), kTestOfflineId1, test_client_id_other_namespace_,
      base::FilePath(kTestFilePath), kFileSize, kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();
  EXPECT_EQ(0UL, added_guids.size());
  // TODO(dimich): we currently don't report updated items since OPM doesn't
  // have support for that. Add as needed, this will have to be updated when
  // support is added.
  EXPECT_EQ(0UL, updated_guids.size());
}

TEST_F(DownloadUIAdapterTest, LoadExistingRequest) {
  AddInitialRequest(GURL(kTestUrl), test_client_id1_);
  OfflineItem item(test_content_id1_);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, AddRequest) {
  AddRequest(GURL(kTestUrl), test_client_id1_);
  EXPECT_EQ(0UL, added_guids.size());
  PumpLoop();
  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(test_client_id1_.id, added_guids[0]);
  OfflineItem item(test_content_id1_);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, RemoveRequest) {
  int64_t id = AddInitialRequest(GURL(kTestUrl), test_client_id1_);
  EXPECT_EQ(1UL, added_guids.size());
  OfflineItem item(test_content_id1_);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));
  EXPECT_EQ(0UL, deleted_guids.size());

  std::vector<int64_t> requests_to_remove = {id};
  request_coordinator()->RemoveRequests(
      requests_to_remove,
      base::BindOnce(
          [](int64_t id, const MultipleItemStatuses& statuses) {
            EXPECT_EQ(1UL, statuses.size());
            EXPECT_EQ(id, statuses[0].first);
            EXPECT_EQ(ItemActionStatus::SUCCESS, statuses[0].second);
          },
          id));
  PumpLoop();

  EXPECT_EQ(1UL, added_guids.size());
  EXPECT_EQ(1UL, deleted_guids.size());
  EXPECT_EQ(test_client_id1_.id, deleted_guids[0]);
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, std::nullopt));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, PauseAndResume) {
  AddRequest(GURL(kTestUrl), test_client_id1_);
  PumpLoop();

  size_t num_updates = updated_guids.size();
  OfflineItem item(test_content_id1_);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));

  // Pause the download. It should fire OnChanged and the item should move to
  // PAUSED.
  adapter->PauseDownload(test_content_id1_);
  PumpLoop();

  EXPECT_GE(updated_guids.size(), num_updates);
  num_updates = updated_guids.size();
  item.state = OfflineItemState::PAUSED;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));

  // Resume the download. It should fire OnChanged again and move the item to
  // IN_PROGRESS.
  adapter->ResumeDownload(test_content_id1_);
  PumpLoop();

  EXPECT_GE(updated_guids.size(), num_updates);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, OnChangedReceivedAfterPageAdded) {
  AddInitialRequest(GURL(kTestUrl), test_client_id1_);
  OfflineItem item(test_content_id1_);
  item.state = OfflineItemState::IN_PROGRESS;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();

  // Add a new saved page with the same client id.
  // This simulates what happens when the request is completed.
  OfflinePageItem page(GURL(kTestUrl), kTestOfflineId1, test_client_id1_,
                       base::FilePath(kTestFilePath), kFileSize,
                       kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();

  item.state = OfflineItemState::COMPLETE;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));

  // Pause the request. It should fire OnChanged, but should not have any effect
  // as the item is already COMPLETE.
  adapter->PauseDownload(test_content_id1_);
  PumpLoop();

  item.state = OfflineItemState::COMPLETE;
  adapter->GetItemById(test_content_id1_,
                       base::BindOnce(&GetItemAndVerify, item));
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, RequestBecomesPage) {
  // This will cause requests to be 'offlined' all the way and removed.
  offliner_stub->enable_callback(true);
  AddInitialRequest(GURL(kTestUrl), test_client_id1_);

  EXPECT_EQ(1UL, added_guids.size());

  // Add a new saved page with the same client id.
  // This simulates what happens when the page is added after the request is
  // completed.
  OfflinePageItem page(GURL(kTestUrl), kTestOfflineId1, test_client_id1_,
                       base::FilePath(kTestFilePath), kFileSize,
                       kTestCreationTime);
  model->AddPageAndNotifyAdapter(page);
  PumpLoop();

  EXPECT_EQ(1UL, added_guids.size());
  // 3 updates: OnChanged for starting request, OnNetworkProgress and
  // OnComplete.
  EXPECT_EQ(3UL, updated_guids.size());

  OfflineItem item(test_content_id1_);
  item.state = OfflineItemState::COMPLETE;
  {
    SCOPED_TRACE("COMPLETE");
    adapter->GetItemById(test_content_id1_,
                         base::BindOnce(&GetItemAndVerify, item));
    PumpLoop();
  }
}

TEST_F(DownloadUIAdapterTest, GetVisualsForItem) {
  AddInitialPage();
  model->visuals_by_offline_id_result =
      std::make_unique<OfflinePageVisuals>(kVisuals);
  const int kImageWidth = 24;
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(kThumbnailData, _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(
            gfx::test::CreateImage(kImageWidth, kImageWidth));
      }));
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(kFaviconData, _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(
            gfx::test::CreateImage(kImageWidth, kImageWidth));
      }));
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](const offline_items_collection::ContentId& id,
          std::unique_ptr<offline_items_collection::OfflineItemVisuals>
              visuals) {
        EXPECT_TRUE(visuals);
        EXPECT_EQ(kImageWidth, visuals->icon.Width());
        EXPECT_EQ(kImageWidth, visuals->custom_favicon.Width());
        called = true;
      });

  base::HistogramTester histogram_tester;
  adapter->GetVisualsForItem(
      test_content_id1_, GetVisualsOptions::IconAndCustomFavicon(), callback);
  PumpLoop();

  EXPECT_TRUE(called);
}

TEST_F(DownloadUIAdapterTest, GetVisualsForItemInvalidItem) {
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(kThumbnailData, _))
      .Times(0);
  AddInitialPage();
  const ContentId kContentID("not", "valid");
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](const offline_items_collection::ContentId& id,
          std::unique_ptr<offline_items_collection::OfflineItemVisuals>
              visuals) {
        EXPECT_EQ(kContentID, id);
        EXPECT_FALSE(visuals);
        called = true;
      });
  base::HistogramTester histogram_tester;

  adapter->GetVisualsForItem(
      kContentID, GetVisualsOptions::IconAndCustomFavicon(), callback);
  PumpLoop();

  EXPECT_TRUE(called);
}

TEST_F(DownloadUIAdapterTest, GetVisualsForItemNoVisuals) {
  AddInitialPage();
  model->visuals_by_offline_id_result = nullptr;
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(_, _)).Times(0);
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(_, _)).Times(0);
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](const offline_items_collection::ContentId& id,
          std::unique_ptr<offline_items_collection::OfflineItemVisuals>
              visuals) {
        EXPECT_EQ(test_content_id1_, id);
        EXPECT_FALSE(visuals);
        called = true;
      });
  adapter->GetAllItems(base::DoNothing());
  base::HistogramTester histogram_tester;

  adapter->GetVisualsForItem(
      test_content_id1_, GetVisualsOptions::IconAndCustomFavicon(), callback);
  PumpLoop();

  EXPECT_TRUE(called);
}

TEST_F(DownloadUIAdapterTest, GetVisualsForItemNoThumbnail) {
  const int kImageWidth = 24;
  AddInitialPage();
  model->visuals_by_offline_id_result =
      std::make_unique<OfflinePageVisuals>(kVisuals);
  model->visuals_by_offline_id_result->thumbnail = "";

  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_("", _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(gfx::test::CreateImage(0, 0));
      }));
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(kFaviconData, _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(
            gfx::test::CreateImage(kImageWidth, kImageWidth));
      }));
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](const offline_items_collection::ContentId& id,
          std::unique_ptr<offline_items_collection::OfflineItemVisuals>
              visuals) {
        EXPECT_EQ(test_content_id1_, id);
        EXPECT_TRUE(visuals);
        called = true;
      });
  adapter->GetAllItems(base::DoNothing());
  base::HistogramTester histogram_tester;

  adapter->GetVisualsForItem(
      test_content_id1_, GetVisualsOptions::IconAndCustomFavicon(), callback);
  PumpLoop();

  EXPECT_TRUE(called);
}

TEST_F(DownloadUIAdapterTest, GetVisualsForItemNoFavicon) {
  const int kImageWidth = 24;
  AddInitialPage();
  model->visuals_by_offline_id_result =
      std::make_unique<OfflinePageVisuals>(kVisuals);
  model->visuals_by_offline_id_result->favicon = "";

  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(kThumbnailData, _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(
            gfx::test::CreateImage(kImageWidth, kImageWidth));
      }));
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_("", _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(gfx::test::CreateImage(0, 0));
      }));
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](const offline_items_collection::ContentId& id,
          std::unique_ptr<offline_items_collection::OfflineItemVisuals>
              visuals) {
        EXPECT_EQ(test_content_id1_, id);
        EXPECT_TRUE(visuals);
        called = true;
      });
  adapter->GetAllItems(base::DoNothing());
  base::HistogramTester histogram_tester;

  adapter->GetVisualsForItem(
      test_content_id1_, GetVisualsOptions::IconAndCustomFavicon(), callback);
  PumpLoop();

  EXPECT_TRUE(called);
}

TEST_F(DownloadUIAdapterTest, GetVisualsForItemBadDecode) {
  AddInitialPage();
  model->visuals_by_offline_id_result =
      std::make_unique<OfflinePageVisuals>(kVisuals);
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(kThumbnailData, _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(gfx::test::CreateImage(0, 0));
      }));
  EXPECT_CALL(*visuals_decoder, DecodeAndCropImage_(kFaviconData, _))
      .WillOnce(WithArg<1>([&](VisualsDecoder::DecodeComplete* callback) {
        std::move(*callback).Run(gfx::test::CreateImage(0, 0));
      }));
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](const offline_items_collection::ContentId& id,
          std::unique_ptr<offline_items_collection::OfflineItemVisuals>
              visuals) {
        EXPECT_EQ(test_content_id1_, id);
        EXPECT_TRUE(visuals);
        called = true;
      });
  base::HistogramTester histogram_tester;

  adapter->GetVisualsForItem(
      test_content_id1_, GetVisualsOptions::IconAndCustomFavicon(), callback);
  PumpLoop();

  EXPECT_TRUE(called);
}

TEST_F(DownloadUIAdapterTest, GetShareInfoForItem) {
  AddInitialPage();

  EXPECT_CALL(*adapter_delegate, GetShareInfoForItem(test_content_id1_, _));
  auto callback = base::BindLambdaForTesting(
      [&](const offline_items_collection::ContentId& id,
          std::unique_ptr<offline_items_collection::OfflineItemShareInfo>
              share_info) {});
  adapter->GetShareInfoForItem(test_content_id1_, callback);
  PumpLoop();
}

TEST_F(DownloadUIAdapterTest, ThumbnailAddedUpdatesItem) {
  // Add an item without a thumbnail. Then notify the adapter about the added
  // thumbnail. It should notify the delegate about the updated item.
  AddInitialPage();
  PumpLoop();
  updated_guids.clear();

  adapter->ThumbnailAdded(model.get(), kTestOfflineId1, std::string());
  EXPECT_EQ(std::vector<std::string>{kTestGuid1}, updated_guids);
}

TEST_F(DownloadUIAdapterTest, ThumbnailAddedItemNotFound) {
  // Notify the adapter about an item not yet loaded. It should be ignored.
  AddInitialPage();
  adapter->GetAllItems(base::DoNothing());
  PumpLoop();
  updated_guids.clear();

  adapter->ThumbnailAdded(model.get(), /*offline_id=*/958120, std::string());
  EXPECT_EQ(std::vector<std::string>{}, updated_guids);
}

}  // namespace
}  // namespace offline_pages
