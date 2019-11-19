// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"

using std::string;
using std::vector;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

using dom_distiller::proto::DomDistillerOptions;
using dom_distiller::proto::DomDistillerResult;
using dom_distiller::proto::DomDistillerResult_ContentImage;
using dom_distiller::proto::TimingEntry;

namespace {
const char kTitle[] = "Title";
const char kContent[] = "Content";
const char kURL[] = "http://a.com/";
const size_t kTotalGoodImages = 2;
const size_t kTotalImages = 3;
// Good images need to be in the front.
const char* kImageURLs[kTotalImages] = {
    "http://a.com/img1.jpg", "http://a.com/img2.jpg", "./bad_url_should_fail"};
const char* kImageData[kTotalImages] = {"abcde", "12345", "VWXYZ"};
const char kDebugLog[] = "Debug Log";

const string GetImageName(int page_num, int image_num) {
  return base::NumberToString(page_num) + "_" + base::NumberToString(image_num);
}

std::unique_ptr<base::Value> CreateDistilledValueReturnedFromJS(
    const string& title,
    const string& content,
    const vector<int>& image_indices,
    const string& next_page_url,
    const string& prev_page_url = "") {
  DomDistillerResult result;
  result.set_title(title);
  result.mutable_distilled_content()->set_html(content);
  result.mutable_pagination_info()->set_next_page(next_page_url);
  result.mutable_pagination_info()->set_prev_page(prev_page_url);

  for (size_t i = 0; i < image_indices.size(); ++i) {
    DomDistillerResult_ContentImage* curr_image = result.add_content_images();
    curr_image->set_url(kImageURLs[image_indices[i]]);
  }

  return dom_distiller::proto::json::DomDistillerResult::WriteToValue(result);
}

// Return the sequence in which Distiller will distill pages.
// Note: ignores any delays due to fetching images etc.
vector<int> GetPagesInSequence(int start_page_num, int num_pages) {
  // Distiller prefers distilling past pages first. E.g. when distillation
  // starts on page 2 then pages are distilled in the order: 2, 1, 0, 3, 4.
  vector<int> page_nums;
  for (int page = start_page_num; page >= 0; --page)
    page_nums.push_back(page);
  for (int page = start_page_num + 1; page < num_pages; ++page)
    page_nums.push_back(page);
  return page_nums;
}

struct MultipageDistillerData {
 public:
  MultipageDistillerData() {}
  ~MultipageDistillerData() {}
  vector<string> page_urls;
  vector<string> content;
  vector<vector<int>> image_ids;
  // The Javascript values returned by mock distiller.
  std::vector<std::unique_ptr<base::Value>> distilled_values;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipageDistillerData);
};

void VerifyIncrementalUpdatesMatch(
    const MultipageDistillerData* distiller_data,
    int num_pages_in_article,
    const vector<dom_distiller::ArticleDistillationUpdate>& incremental_updates,
    int start_page_num) {
  vector<int> page_seq =
      GetPagesInSequence(start_page_num, num_pages_in_article);
  // Updates should contain a list of pages. Pages in an update should be in
  // the correct ascending page order regardless of |start_page_num|.
  // E.g. if distillation starts at page 2 of a 3 page article, the updates
  // will be [[2], [1, 2], [1, 2, 3]]. This example assumes that image fetches
  // do not delay distillation of a page. There can be scenarios when image
  // fetch delays distillation of a page (E.g. 1 is delayed due to image
  // fetches so updates can be in this order [[2], [2,3], [1,2,3]].
  for (size_t update_count = 0; update_count < incremental_updates.size();
       ++update_count) {
    const dom_distiller::ArticleDistillationUpdate& update =
        incremental_updates[update_count];
    EXPECT_EQ(update_count + 1, update.GetPagesSize());

    vector<int> expected_page_nums_in_update(
        page_seq.begin(), page_seq.begin() + update.GetPagesSize());
    std::sort(expected_page_nums_in_update.begin(),
              expected_page_nums_in_update.end());

    // If we already got the first page then there is no previous page.
    EXPECT_EQ((expected_page_nums_in_update[0] != 0), update.HasPrevPage());

    // if we already got the last page then there is no next page.
    EXPECT_EQ(
        (*expected_page_nums_in_update.rbegin()) != num_pages_in_article - 1,
        update.HasNextPage());
    for (size_t j = 0; j < update.GetPagesSize(); ++j) {
      int actual_page_num = expected_page_nums_in_update[j];
      EXPECT_EQ(distiller_data->page_urls[actual_page_num],
                update.GetDistilledPage(j).url());
      EXPECT_EQ(distiller_data->content[actual_page_num],
                update.GetDistilledPage(j).html());
    }
  }
}

string GenerateNextPageUrl(const std::string& url_prefix,
                           size_t page_num,
                           size_t pages_size) {
  return page_num + 1 < pages_size
             ? url_prefix + base::NumberToString(page_num + 1)
             : "";
}

string GeneratePrevPageUrl(const std::string& url_prefix, size_t page_num) {
  return page_num > 0 ? url_prefix + base::NumberToString(page_num - 1) : "";
}

std::unique_ptr<MultipageDistillerData> CreateMultipageDistillerDataWithImages(
    const vector<vector<int>>& image_ids) {
  size_t pages_size = image_ids.size();
  std::unique_ptr<MultipageDistillerData> result(new MultipageDistillerData());
  string url_prefix = kURL;
  result->image_ids = image_ids;
  for (size_t page_num = 0; page_num < pages_size; ++page_num) {
    result->page_urls.push_back(url_prefix + base::NumberToString(page_num));
    result->content.push_back("Content for page:" +
                              base::NumberToString(page_num));
    string next_page_url =
        GenerateNextPageUrl(url_prefix, page_num, pages_size);
    string prev_page_url = GeneratePrevPageUrl(url_prefix, page_num);
    std::unique_ptr<base::Value> distilled_value =
        CreateDistilledValueReturnedFromJS(kTitle, result->content[page_num],
                                           image_ids[page_num], next_page_url,
                                           prev_page_url);
    result->distilled_values.push_back(std::move(distilled_value));
  }
  return result;
}

std::unique_ptr<MultipageDistillerData>
CreateMultipageDistillerDataWithoutImages(size_t pages_size) {
  return CreateMultipageDistillerDataWithImages(
      vector<vector<int>>(pages_size));
}

void VerifyArticleProtoMatchesMultipageData(
    const dom_distiller::DistilledArticleProto* article_proto,
    const MultipageDistillerData* distiller_data,
    size_t distilled_pages_size,
    size_t total_pages_size) {
  ASSERT_EQ(distilled_pages_size,
            static_cast<size_t>(article_proto->pages_size()));
  EXPECT_EQ(kTitle, article_proto->title());
  std::string url_prefix = kURL;
  for (size_t page_num = 0; page_num < distilled_pages_size; ++page_num) {
    const dom_distiller::DistilledPageProto& page =
        article_proto->pages(page_num);
    EXPECT_EQ(distiller_data->content[page_num], page.html());
    EXPECT_EQ(distiller_data->page_urls[page_num], page.url());
    EXPECT_EQ(distiller_data->image_ids[page_num].size(),
              static_cast<size_t>(page.image_size()));
    const vector<int>& image_ids_for_page = distiller_data->image_ids[page_num];
    for (size_t img_num = 0; img_num < image_ids_for_page.size(); ++img_num) {
      if (dom_distiller::DistillerImpl::DoesFetchImages()) {
        EXPECT_EQ(kImageData[image_ids_for_page[img_num]],
                  page.image(img_num).data());
      } else {
        EXPECT_EQ("", page.image(img_num).data());
      }
      EXPECT_EQ(GetImageName(page_num + 1, img_num),
                page.image(img_num).name());
    }
    std::string expected_next_page_url =
        GenerateNextPageUrl(url_prefix, page_num, total_pages_size);
    std::string expected_prev_page_url =
        GeneratePrevPageUrl(url_prefix, page_num);
    EXPECT_EQ(expected_next_page_url, page.pagination_info().next_page());
    EXPECT_EQ(expected_prev_page_url, page.pagination_info().prev_page());
    EXPECT_FALSE(page.pagination_info().has_canonical_page());
  }
}

}  // namespace

namespace dom_distiller {

using test::MockDistillerPage;
using test::MockDistillerPageFactory;

class TestDistillerURLFetcher : public DistillerURLFetcher {
 public:
  explicit TestDistillerURLFetcher(bool delay_fetch)
      : DistillerURLFetcher(nullptr), delay_fetch_(delay_fetch) {
    responses_[kImageURLs[0]] = string(kImageData[0]);
    responses_[kImageURLs[1]] = string(kImageData[1]);
  }

  void FetchURL(const string& url,
                const URLFetcherCallback& callback) override {
    ASSERT_FALSE(callback.is_null());
    url_ = url;
    callback_ = callback;
    if (!delay_fetch_) {
      PostCallbackTask();
    }
  }

  void PostCallbackTask() {
    ASSERT_TRUE(base::MessageLoopCurrent::Get());
    ASSERT_FALSE(callback_.is_null());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback_, responses_[url_]));
  }

 private:
  std::map<string, string> responses_;
  string url_;
  URLFetcherCallback callback_;
  bool delay_fetch_;
};

class TestDistillerURLFetcherFactory : public DistillerURLFetcherFactory {
 public:
  TestDistillerURLFetcherFactory() : DistillerURLFetcherFactory(nullptr) {}

  ~TestDistillerURLFetcherFactory() override {}
  DistillerURLFetcher* CreateDistillerURLFetcher() const override {
    return new TestDistillerURLFetcher(false);
  }
};

class MockDistillerURLFetcherFactory : public DistillerURLFetcherFactory {
 public:
  MockDistillerURLFetcherFactory() : DistillerURLFetcherFactory(nullptr) {}
  ~MockDistillerURLFetcherFactory() override {}

  MOCK_CONST_METHOD0(CreateDistillerURLFetcher, DistillerURLFetcher*());
};

class DistillerTest : public testing::Test {
 public:
  ~DistillerTest() override {}

  void OnDistillArticleDone(std::unique_ptr<DistilledArticleProto> proto) {
    article_proto_ = std::move(proto);
  }

  void OnDistillArticleUpdate(const ArticleDistillationUpdate& article_update) {
    in_sequence_updates_.push_back(article_update);
  }

  void DistillPage(const std::string& url,
                   std::unique_ptr<DistillerPage> distiller_page) {
    distiller_->DistillPage(GURL(url), std::move(distiller_page),
                            base::Bind(&DistillerTest::OnDistillArticleDone,
                                       base::Unretained(this)),
                            base::Bind(&DistillerTest::OnDistillArticleUpdate,
                                       base::Unretained(this)));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<DistillerImpl> distiller_;
  std::unique_ptr<DistilledArticleProto> article_proto_;
  std::vector<ArticleDistillationUpdate> in_sequence_updates_;
  MockDistillerPageFactory page_factory_;
  TestDistillerURLFetcherFactory url_fetcher_factory_;
};

ACTION_P3(DistillerPageOnDistillationDone, distiller_page, url, result) {
  distiller_page->OnDistillationDone(url, result);
}

std::unique_ptr<DistillerPage> CreateMockDistillerPage(
    const base::Value* result,
    const GURL& url) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  EXPECT_CALL(*distiller_page, DistillPageImpl(url, _))
      .WillOnce(DistillerPageOnDistillationDone(distiller_page, url, result));
  return std::unique_ptr<DistillerPage>(distiller_page);
}

std::unique_ptr<DistillerPage> CreateMockDistillerPageWithPendingJSCallback(
    MockDistillerPage** distiller_page_ptr,
    const GURL& url) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  *distiller_page_ptr = distiller_page;
  EXPECT_CALL(*distiller_page, DistillPageImpl(url, _));
  return std::unique_ptr<DistillerPage>(distiller_page);
}

std::unique_ptr<DistillerPage> CreateMockDistillerPages(
    MultipageDistillerData* distiller_data,
    size_t pages_size,
    int start_page_num) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  {
    testing::InSequence s;
    vector<int> page_nums = GetPagesInSequence(start_page_num, pages_size);
    for (size_t page_num = 0; page_num < pages_size; ++page_num) {
      int page = page_nums[page_num];
      GURL url = GURL(distiller_data->page_urls[page]);
      EXPECT_CALL(*distiller_page, DistillPageImpl(url, _))
          .WillOnce(DistillerPageOnDistillationDone(
              distiller_page, url,
              distiller_data->distilled_values[page].get()));
    }
  }
  return std::unique_ptr<DistillerPage>(distiller_page);
}

TEST_F(DistillerTest, DistillPage) {
  std::unique_ptr<base::Value> result =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), "");
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPage(result.get(), GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  ASSERT_EQ(article_proto_->pages_size(), 1);
  const DistilledPageProto& first_page = article_proto_->pages(0);
  EXPECT_EQ(kContent, first_page.html());
  EXPECT_EQ(kURL, first_page.url());
}

TEST_F(DistillerTest, DistillPageWithDebugInfo) {
  DomDistillerResult dd_result;
  dd_result.mutable_debug_info()->set_log(kDebugLog);
  std::unique_ptr<base::Value> result =
      dom_distiller::proto::json::DomDistillerResult::WriteToValue(dd_result);
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPage(result.get(), GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  const DistilledPageProto& first_page = article_proto_->pages(0);
  EXPECT_EQ(kDebugLog, first_page.debug_info().log());
}

void SetTimingEntry(TimingEntry* entry, const std::string& name, double time) {
  entry->set_name(name);
  entry->set_time(time);
}

TEST_F(DistillerTest, DistillPageWithTimingInfo) {
  DomDistillerResult dd_result;
  dd_result.mutable_timing_info()->set_total_time(1.0);
  dd_result.mutable_timing_info()->set_markup_parsing_time(2.0);
  dd_result.mutable_timing_info()->set_document_construction_time(3.0);
  dd_result.mutable_timing_info()->set_article_processing_time(4.0);
  dd_result.mutable_timing_info()->set_formatting_time(5.0);
  SetTimingEntry(dd_result.mutable_timing_info()->add_other_times(), "time0",
                 6.0);
  SetTimingEntry(dd_result.mutable_timing_info()->add_other_times(), "time1",
                 7.0);
  std::unique_ptr<base::Value> result =
      dom_distiller::proto::json::DomDistillerResult::WriteToValue(dd_result);
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPage(result.get(), GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  const DistilledPageProto& first_page = article_proto_->pages(0);
  std::map<std::string, double> timings;
  for (int i = 0; i < first_page.timing_info_size(); ++i) {
    DistilledPageProto::TimingInfo timing = first_page.timing_info(i);
    timings[timing.name()] = timing.time();
  }
  EXPECT_EQ(7u, timings.size());
  EXPECT_EQ(1.0, timings["total"]);
  EXPECT_EQ(2.0, timings["markup_parsing"]);
  EXPECT_EQ(3.0, timings["document_construction"]);
  EXPECT_EQ(4.0, timings["article_processing"]);
  EXPECT_EQ(5.0, timings["formatting"]);
  EXPECT_EQ(6.0, timings["time0"]);
  EXPECT_EQ(7.0, timings["time1"]);
}

TEST_F(DistillerTest, DistillPageWithImages) {
  vector<int> image_indices;
  image_indices.push_back(0);
  image_indices.push_back(1);
  image_indices.push_back(2);
  std::unique_ptr<base::Value> result =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, image_indices, "");
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPage(result.get(), GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  ASSERT_EQ(article_proto_->pages_size(), 1);
  const DistilledPageProto& first_page = article_proto_->pages(0);
  EXPECT_EQ(kContent, first_page.html());
  EXPECT_EQ(kURL, first_page.url());
  ASSERT_EQ(2, first_page.image_size());

  if (DistillerImpl::DoesFetchImages()) {
    EXPECT_EQ(kImageData[0], first_page.image(0).data());
  } else {
    EXPECT_EQ("", first_page.image(0).data());
  }
  EXPECT_EQ(kImageURLs[0], first_page.image(0).url());
  EXPECT_EQ(GetImageName(1, 0), first_page.image(0).name());

  if (DistillerImpl::DoesFetchImages()) {
    EXPECT_EQ(kImageData[1], first_page.image(1).data());
  } else {
    EXPECT_EQ("", first_page.image(1).data());
  }
  EXPECT_EQ(kImageURLs[1], first_page.image(1).url());
  EXPECT_EQ(GetImageName(1, 1), first_page.image(1).name());
}

TEST_F(DistillerTest, DistillMultiplePages) {
  const size_t kNumPages = 8;

  // Add images.
  vector<vector<int>> image_ids;
  int next_image_number = 0;
  for (size_t page_num = 0; page_num < kNumPages; ++page_num) {
    // Each page has different number of images.
    size_t tot_images = (page_num + kTotalImages) % (kTotalImages + 1);
    vector<int> image_indices;
    for (size_t img_num = 0; img_num < tot_images; img_num++) {
      image_indices.push_back(next_image_number);
      next_image_number = (next_image_number + 1) % kTotalGoodImages;
    }
    image_ids.push_back(image_indices);
  }

  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithImages(image_ids);

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(distiller_data->page_urls[0],
              CreateMockDistillerPages(distiller_data.get(), kNumPages, 0));
  base::RunLoop().RunUntilIdle();
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), kNumPages, kNumPages);
}

TEST_F(DistillerTest, DistillLinkLoop) {
  // Create a loop, the next page is same as the current page. This could
  // happen if javascript misparses a next page link.
  std::unique_ptr<base::Value> result =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), kURL);
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPage(result.get(), GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(article_proto_->pages_size(), 1);
}

TEST_F(DistillerTest, CheckMaxPageLimitExtraPage) {
  const size_t kMaxPagesInArticle = 10;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kMaxPagesInArticle);

  // Note: Next page url of the last page of article is set. So distiller will
  // try to do kMaxPagesInArticle + 1 calls if the max article limit does not
  // work.
  std::unique_ptr<base::Value> last_page_data =
      CreateDistilledValueReturnedFromJS(
          kTitle, distiller_data->content[kMaxPagesInArticle - 1],
          vector<int>(), "", distiller_data->page_urls[kMaxPagesInArticle - 2]);

  distiller_data->distilled_values.pop_back();
  distiller_data->distilled_values.push_back(std::move(last_page_data));

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));

  distiller_->SetMaxNumPagesInArticle(kMaxPagesInArticle);

  DistillPage(
      distiller_data->page_urls[0],
      CreateMockDistillerPages(distiller_data.get(), kMaxPagesInArticle, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kMaxPagesInArticle,
            static_cast<size_t>(article_proto_->pages_size()));
}

TEST_F(DistillerTest, CheckMaxPageLimitExactLimit) {
  const size_t kMaxPagesInArticle = 10;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kMaxPagesInArticle);

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));

  // Check if distilling an article with exactly the page limit works.
  distiller_->SetMaxNumPagesInArticle(kMaxPagesInArticle);

  DistillPage(
      distiller_data->page_urls[0],
      CreateMockDistillerPages(distiller_data.get(), kMaxPagesInArticle, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kMaxPagesInArticle,
            static_cast<size_t>(article_proto_->pages_size()));
}

TEST_F(DistillerTest, SinglePageDistillationFailure) {
  // To simulate failure return a null value.
  auto null_value = std::make_unique<base::Value>();
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPage(null_value.get(), GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", article_proto_->title());
  EXPECT_EQ(0, article_proto_->pages_size());
}

TEST_F(DistillerTest, MultiplePagesDistillationFailure) {
  const size_t kNumPages = 8;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  // The page number of the failed page.
  size_t failed_page_num = 3;
  // reset distilled data of the failed page.
  distiller_data->distilled_values.erase(
      distiller_data->distilled_values.begin() + failed_page_num);
  distiller_data->distilled_values.insert(
      distiller_data->distilled_values.begin() + failed_page_num,
      std::make_unique<base::Value>());
  // Expect only calls till the failed page number.
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(
      distiller_data->page_urls[0],
      CreateMockDistillerPages(distiller_data.get(), failed_page_num + 1, 0));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), failed_page_num, kNumPages);
}

TEST_F(DistillerTest, DistillMultiplePagesFirstEmpty) {
  const size_t kNumPages = 8;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  // The first page has no content.
  const size_t empty_page_num = 0;
  distiller_data->content[empty_page_num] = "";
  std::unique_ptr<base::Value> distilled_value =
      CreateDistilledValueReturnedFromJS(
          kTitle, "", vector<int>(),
          GenerateNextPageUrl(kURL, empty_page_num, kNumPages),
          GeneratePrevPageUrl(kURL, empty_page_num));
  // Reset distilled data of the first page.
  distiller_data->distilled_values.erase(
      distiller_data->distilled_values.begin() + empty_page_num);
  distiller_data->distilled_values.insert(
      distiller_data->distilled_values.begin() + empty_page_num,
      std::move(distilled_value));

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(distiller_data->page_urls[0],
              CreateMockDistillerPages(distiller_data.get(), 1, 0));
  base::RunLoop().RunUntilIdle();
  // If the first page has no content, stop fetching the next page.
  EXPECT_EQ(1, article_proto_->pages_size());
  VerifyArticleProtoMatchesMultipageData(article_proto_.get(),
                                         distiller_data.get(), 1, 1);
}

TEST_F(DistillerTest, DistillMultiplePagesSecondEmpty) {
  const size_t kNumPages = 8;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  // The second page has no content.
  const size_t empty_page_num = 1;
  distiller_data->content[empty_page_num] = "";
  std::unique_ptr<base::Value> distilled_value =
      CreateDistilledValueReturnedFromJS(
          kTitle, "", vector<int>(),
          GenerateNextPageUrl(kURL, empty_page_num, kNumPages),
          GeneratePrevPageUrl(kURL, empty_page_num));
  // Reset distilled data of the second page.
  distiller_data->distilled_values.erase(
      distiller_data->distilled_values.begin() + empty_page_num);
  distiller_data->distilled_values.insert(
      distiller_data->distilled_values.begin() + empty_page_num,
      std::move(distilled_value));

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(distiller_data->page_urls[0],
              CreateMockDistillerPages(distiller_data.get(), kNumPages, 0));
  base::RunLoop().RunUntilIdle();

  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), kNumPages, kNumPages);
}

TEST_F(DistillerTest, DistillPreviousPage) {
  const size_t kNumPages = 8;

  // The page number of the article on which distillation starts.
  int start_page_num = 3;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(distiller_data->page_urls[start_page_num],
              CreateMockDistillerPages(distiller_data.get(), kNumPages,
                                       start_page_num));
  base::RunLoop().RunUntilIdle();
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), kNumPages, kNumPages);
}

TEST_F(DistillerTest, IncrementalUpdates) {
  const size_t kNumPages = 8;

  // The page number of the article on which distillation starts.
  int start_page_num = 3;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(distiller_data->page_urls[start_page_num],
              CreateMockDistillerPages(distiller_data.get(), kNumPages,
                                       start_page_num));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  ASSERT_EQ(kNumPages, static_cast<size_t>(article_proto_->pages_size()));
  EXPECT_EQ(kNumPages, in_sequence_updates_.size());

  VerifyIncrementalUpdatesMatch(distiller_data.get(), kNumPages,
                                in_sequence_updates_, start_page_num);
}

TEST_F(DistillerTest, IncrementalUpdatesDoNotDeleteFinalArticle) {
  const size_t kNumPages = 8;
  int start_page_num = 3;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(distiller_data->page_urls[start_page_num],
              CreateMockDistillerPages(distiller_data.get(), kNumPages,
                                       start_page_num));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNumPages, in_sequence_updates_.size());

  in_sequence_updates_.clear();

  // Should still be able to access article and pages.
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), kNumPages, kNumPages);
}

TEST_F(DistillerTest, DeletingArticleDoesNotInterfereWithUpdates) {
  const size_t kNumPages = 8;
  std::unique_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);
  // The page number of the article on which distillation starts.
  int start_page_num = 3;

  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(distiller_data->page_urls[start_page_num],
              CreateMockDistillerPages(distiller_data.get(), kNumPages,
                                       start_page_num));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNumPages, in_sequence_updates_.size());
  EXPECT_EQ(kTitle, article_proto_->title());
  ASSERT_EQ(kNumPages, static_cast<size_t>(article_proto_->pages_size()));

  // Delete the article.
  article_proto_.reset();
  VerifyIncrementalUpdatesMatch(distiller_data.get(), kNumPages,
                                in_sequence_updates_, start_page_num);
}

TEST_F(DistillerTest, CancelWithDelayedImageFetchCallback) {
  if (!DistillerImpl::DoesFetchImages())
    return;

  vector<int> image_indices;
  image_indices.push_back(0);
  std::unique_ptr<base::Value> distilled_value =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, image_indices, "");
  TestDistillerURLFetcher* delayed_fetcher = new TestDistillerURLFetcher(true);
  MockDistillerURLFetcherFactory mock_url_fetcher_factory;
  EXPECT_CALL(mock_url_fetcher_factory, CreateDistillerURLFetcher())
      .WillOnce(Return(delayed_fetcher));
  distiller_.reset(
      new DistillerImpl(mock_url_fetcher_factory, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPage(distilled_value.get(), GURL(kURL)));
  base::RunLoop().RunUntilIdle();

  // Post callback from the url fetcher and then delete the distiller.
  delayed_fetcher->PostCallbackTask();
  distiller_.reset();

  base::RunLoop().RunUntilIdle();
}

TEST_F(DistillerTest, CancelWithDelayedJSCallback) {
  std::unique_ptr<base::Value> distilled_value =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), "");
  MockDistillerPage* distiller_page = nullptr;
  distiller_.reset(
      new DistillerImpl(url_fetcher_factory_, DomDistillerOptions()));
  DistillPage(kURL, CreateMockDistillerPageWithPendingJSCallback(
                        &distiller_page, GURL(kURL)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(distiller_page);
  // Post the task to execute javascript and then delete the distiller.
  distiller_page->OnDistillationDone(GURL(kURL), distilled_value.get());
  distiller_.reset();

  base::RunLoop().RunUntilIdle();
}

}  // namespace dom_distiller
