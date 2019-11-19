// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/get_pages_task.h"

#include <stdint.h>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const char kTestNamespace[] = "test_namespace";

}  // namespace

class GetPagesTaskTest : public ModelTaskTestBase {
 public:
  const std::set<OfflinePageItem>& task_result() const { return task_result_; }
  const std::vector<OfflinePageItem>& ordered_task_result() const {
    return ordered_task_result_;
  }

  void InsertItems(std::vector<OfflinePageItem> items) {
    for (auto& item : items) {
      store_test_util()->InsertItem(item);
    }
  }

  std::unique_ptr<GetPagesTask> CreateTask(const PageCriteria& criteria) {
    return std::make_unique<GetPagesTask>(
        store(), criteria,
        base::BindOnce(&GetPagesTaskTest::OnGetPagesDone,
                       base::Unretained(this)));
  }

 private:
  void OnGetPagesDone(const std::vector<OfflinePageItem>& result) {
    ordered_task_result_ = result;
    task_result_.clear();
    task_result_.insert(result.begin(), result.end());
    // Verify there were no identical items, to ensure the set contains the
    // same data.
    EXPECT_EQ(result.size(), task_result_.size());
  }

 protected:
  std::set<OfflinePageItem> task_result_;
  std::vector<OfflinePageItem> ordered_task_result_;
};

TEST_F(GetPagesTaskTest, GetAllPages) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem item_1 = generator()->CreateItem();
  OfflinePageItem item_2 = generator()->CreateItem();
  OfflinePageItem item_3 = generator()->CreateItem();
  InsertItems({item_1, item_2, item_3});

  RunTask(CreateTask(PageCriteria()));

  EXPECT_EQ(std::set<OfflinePageItem>({item_1, item_2, item_3}), task_result());
}

TEST_F(GetPagesTaskTest, ClientId) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem item_1 = generator()->CreateItem();
  OfflinePageItem item_2 = generator()->CreateItem();
  OfflinePageItem item_3 = generator()->CreateItem();
  InsertItems({item_1, item_2, item_3});

  PageCriteria criteria;

  criteria.client_ids =
      std::vector<ClientId>{item_1.client_id, item_2.client_id};
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::set<OfflinePageItem>({item_1, item_2}), task_result());

  criteria.client_ids = std::vector<ClientId>();
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::set<OfflinePageItem>(), task_result());
}

TEST_F(GetPagesTaskTest, Namespace) {
  static const char kOtherNamespace[] = "other_namespace";
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem item_1 = generator()->CreateItem();
  OfflinePageItem item_2 = generator()->CreateItem();
  generator()->SetNamespace(kOtherNamespace);
  OfflinePageItem item_3 = generator()->CreateItem();
  InsertItems({item_1, item_2, item_3});

  PageCriteria criteria;

  criteria.client_namespaces = std::vector<std::string>{kTestNamespace};
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::set<OfflinePageItem>({item_1, item_2}), task_result());

  criteria.client_namespaces = std::vector<std::string>{};
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::set<OfflinePageItem>(), task_result());
}

TEST_F(GetPagesTaskTest, RequestOrigin) {
  static const char kRequestOrigin1[] = "bar";
  static const char kRequestOrigin2[] = "baz";
  generator()->SetNamespace(kTestNamespace);
  generator()->SetRequestOrigin(kRequestOrigin1);
  OfflinePageItem item_1 = generator()->CreateItem();
  OfflinePageItem item_2 = generator()->CreateItem();
  generator()->SetRequestOrigin(kRequestOrigin2);
  OfflinePageItem item_3 = generator()->CreateItem();
  InsertItems({item_1, item_2, item_3});

  PageCriteria criteria;
  criteria.request_origin = kRequestOrigin1;
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::set<OfflinePageItem>({item_1, item_2}), task_result());
}

TEST_F(GetPagesTaskTest, Url) {
  const GURL kUrl1("http://cs.chromium.org");
  const GURL kUrl1WithSuffix("http://cs.chromium.org/suffix");
  const GURL kUrl1Frag("http://cs.chromium.org#frag1");
  const GURL kUrl2("http://chrome.google.com");
  generator()->SetNamespace(kTestNamespace);
  generator()->SetUrl(kUrl1);
  OfflinePageItem item_1 = generator()->CreateItem();

  generator()->SetUrl(kUrl1Frag);
  OfflinePageItem item_2 = generator()->CreateItem();

  generator()->SetUrl(kUrl2);
  generator()->SetOriginalUrl(kUrl1);
  OfflinePageItem item_3 = generator()->CreateItem();

  generator()->SetUrl(kUrl2);
  generator()->SetOriginalUrl(kUrl1Frag);
  OfflinePageItem item_4 = generator()->CreateItem();

  generator()->SetUrl(kUrl2);
  generator()->SetOriginalUrl(kUrl2);
  OfflinePageItem item_5 = generator()->CreateItem();

  generator()->SetUrl(kUrl1WithSuffix);
  generator()->SetOriginalUrl(kUrl1WithSuffix);
  OfflinePageItem item_6 = generator()->CreateItem();

  InsertItems({item_1, item_2, item_3, item_4, item_5, item_6});

  PageCriteria criteria;
  criteria.url = kUrl1;
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::set<OfflinePageItem>({item_1, item_2, item_3, item_4}),
            task_result());
}

TEST_F(GetPagesTaskTest, OfflineId) {
  generator()->SetNamespace(kTestNamespace);
  const OfflinePageItem item_1 = generator()->CreateItem();
  const OfflinePageItem item_2 = generator()->CreateItem();
  const OfflinePageItem item_3 = generator()->CreateItem();
  InsertItems({item_1, item_2, item_3});

  PageCriteria criteria;

  criteria.offline_ids = std::vector<int64_t>{item_1.offline_id};
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::set<OfflinePageItem>({item_1}), task_result());

  criteria.offline_ids = std::vector<int64_t>{};
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::set<OfflinePageItem>({}), task_result());
}

TEST_F(GetPagesTaskTest, Guid) {
  const OfflinePageItem item_1 = generator()->CreateItem();
  const OfflinePageItem item_2 = generator()->CreateItem();
  const OfflinePageItem item_3 = generator()->CreateItem();
  InsertItems({item_1, item_2, item_3});

  PageCriteria criteria;
  criteria.guid = item_1.client_id.id;
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::set<OfflinePageItem>({item_1}), task_result());
}

TEST_F(GetPagesTaskTest, FileSize) {
  OfflinePageItem item_1 = generator()->CreateItem();
  item_1.file_size = 123;
  OfflinePageItem item_2 = generator()->CreateItem();
  InsertItems({item_1, item_2});

  PageCriteria criteria;
  criteria.file_size = 123;
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::set<OfflinePageItem>({item_1}), task_result());
}

TEST_F(GetPagesTaskTest, Digest) {
  OfflinePageItem item_1 = generator()->CreateItem();
  item_1.digest = "abc";
  OfflinePageItem item_2 = generator()->CreateItem();
  item_2.digest = "123";
  InsertItems({item_1, item_2});

  PageCriteria criteria;
  criteria.digest = "abc";
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::set<OfflinePageItem>({item_1}), task_result());
}

TEST_F(GetPagesTaskTest, MultipleConditions) {
  const GURL kUrl1("http://cs.chromium.org");
  const std::string digest = "abc";

  // |item_1| matches, and all other items differ by one criteria.
  OfflinePageItem item_1 = generator()->CreateItem();
  item_1.digest = digest;
  item_1.file_size = 123;
  item_1.url = kUrl1;
  item_1.client_id.name_space = kDownloadNamespace;

  OfflinePageItem item_2 = item_1;
  item_2.offline_id = store_utils::GenerateOfflineId();
  item_2.digest = "other";

  OfflinePageItem item_3 = item_1;
  item_3.offline_id = store_utils::GenerateOfflineId();
  item_3.client_id.name_space = kLastNNamespace;

  OfflinePageItem item_4 = item_1;
  item_4.offline_id = store_utils::GenerateOfflineId();
  item_4.file_size = 0;

  OfflinePageItem item_5 = item_1;
  item_5.offline_id = store_utils::GenerateOfflineId();
  item_5.url = GURL("http://cs.chromium.org/1");

  InsertItems({item_1, item_2, item_3, item_4, item_5});

  PageCriteria criteria;
  criteria.digest = digest;
  criteria.file_size = 123;
  criteria.url = kUrl1;
  criteria.exclude_tab_bound_pages = true;
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::set<OfflinePageItem>({item_1}), task_result());
}

TEST_F(GetPagesTaskTest, SupportedByDownloads) {
  generator()->SetNamespace(kCCTNamespace);
  store_test_util()->InsertItem(generator()->CreateItem());
  generator()->SetNamespace(kDownloadNamespace);
  OfflinePageItem download_item = generator()->CreateItem();
  store_test_util()->InsertItem(download_item);
  generator()->SetNamespace(kNTPSuggestionsNamespace);
  OfflinePageItem ntp_suggestion_item = generator()->CreateItem();
  store_test_util()->InsertItem(ntp_suggestion_item);

  // All pages with supported_by_downloads.
  {
    PageCriteria criteria;
    criteria.supported_by_downloads = true;
    RunTask(CreateTask(criteria));

    EXPECT_EQ(std::set<OfflinePageItem>({download_item, ntp_suggestion_item}),
              task_result());
  }

  // Only CCT, NTP with supported_by_downloads.
  {
    PageCriteria criteria;
    criteria.supported_by_downloads = true;
    criteria.client_namespaces =
        std::vector<std::string>{kCCTNamespace, kNTPSuggestionsNamespace};
    RunTask(CreateTask(criteria));

    EXPECT_EQ(std::set<OfflinePageItem>({ntp_suggestion_item}), task_result());
  }
}

TEST_F(GetPagesTaskTest, RemovedOnCacheReset) {
  generator()->SetNamespace(kCCTNamespace);
  OfflinePageItem cct_item = generator()->CreateItem();
  store_test_util()->InsertItem(cct_item);
  generator()->SetNamespace(kDownloadNamespace);
  store_test_util()->InsertItem(generator()->CreateItem());
  generator()->SetNamespace(kNTPSuggestionsNamespace);
  store_test_util()->InsertItem(generator()->CreateItem());

  PageCriteria criteria;
  criteria.lifetime_type = LifetimeType::TEMPORARY;
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::set<OfflinePageItem>({cct_item}), task_result());
}

TEST_F(GetPagesTaskTest, OrderDescendingCreationTime) {
  generator()->SetNamespace(kCCTNamespace);
  OfflinePageItem item1 = generator()->CreateItem();
  OfflinePageItem item2 = generator()->CreateItem();
  item2.creation_time = item1.creation_time + base::TimeDelta::FromSeconds(2);
  OfflinePageItem item3 = generator()->CreateItem();
  item3.creation_time = item1.creation_time + base::TimeDelta::FromSeconds(1);

  InsertItems({item1, item2, item3});

  PageCriteria criteria;
  // kDescendingCreationTime is default.
  RunTask(CreateTask(criteria));

  EXPECT_EQ(std::vector<OfflinePageItem>({item2, item3, item1}),
            ordered_task_result());
}

TEST_F(GetPagesTaskTest, OrderAccessTime) {
  generator()->SetNamespace(kCCTNamespace);
  OfflinePageItem item1 = generator()->CreateItem();
  item1.last_access_time = base::Time();
  OfflinePageItem item2 = generator()->CreateItem();
  item2.last_access_time = base::Time() + base::TimeDelta::FromSeconds(2);
  OfflinePageItem item3 = generator()->CreateItem();
  item3.last_access_time = base::Time() + base::TimeDelta::FromSeconds(1);

  InsertItems({item1, item2, item3});

  PageCriteria criteria;

  criteria.result_order = PageCriteria::kAscendingAccessTime;
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::vector<OfflinePageItem>({item1, item3, item2}),
            ordered_task_result());

  criteria.result_order = PageCriteria::kDescendingAccessTime;
  RunTask(CreateTask(criteria));
  EXPECT_EQ(std::vector<OfflinePageItem>({item2, item3, item1}),
            ordered_task_result());
}

}  // namespace offline_pages
