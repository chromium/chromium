// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/extraction_utils.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

using dom_distiller::DistillerOptions;

namespace {
const char kTitle[] = "Title";
const char kContent[] = "Content";
const char kTextContent[] = "Text content";
const char kTextDirection[] = "ltr";
const char kURL[] = "http://a.com/";

base::Value CreateDistilledValueReturnedFromJS(
    const std::string& title,
    const std::string& content,
    const std::string& text_content = kTextContent,
    std::optional<std::string> text_direction = kTextDirection) {
  base::DictValue result;
  result.Set(dom_distiller::kDistilledTitleKey, title);
  result.Set(dom_distiller::kDistilledContentKey, content);
  result.Set(dom_distiller::kDistilledTextContentKey, text_content);
  if (text_direction.has_value()) {
    result.Set(dom_distiller::kDistilledTextDirectionKey, *text_direction);
  }
  return base::Value(std::move(result));
}

}  // namespace

namespace dom_distiller {

using test::MockDistillerPage;

class DistillerTest : public testing::Test {
 public:
  ~DistillerTest() override = default;

  void OnDistillArticleDone(std::unique_ptr<DistilledArticleProto> proto,
                            DistillationParseResult /*result*/) {
    article_proto_ = std::move(proto);
  }

  void OnDistillArticleUpdate(const ArticleDistillationUpdate& article_update) {
    in_sequence_updates_.push_back(article_update);
  }

  void DistillPage(const std::string& url,
                   std::unique_ptr<DistillerPage> distiller_page) {
    distiller_->DistillPage(
        GURL(url), std::move(distiller_page),
        base::BindOnce(&DistillerTest::OnDistillArticleDone,
                       base::Unretained(this)),
        base::BindRepeating(&DistillerTest::OnDistillArticleUpdate,
                            base::Unretained(this)));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<DistillerImpl> distiller_;
  std::unique_ptr<DistilledArticleProto> article_proto_;
  std::vector<ArticleDistillationUpdate> in_sequence_updates_;
};

ACTION_P3(DistillerPageOnDistillationDone, distiller_page, url, result) {
  distiller_page->OnDistillationDone(url, result);
}

std::unique_ptr<DistillerPage> CreateMockDistillerPage(
    const base::Value* result,
    const GURL& url) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  distiller_page->SetMinimumAllowableDistilledContentLength(0);
  EXPECT_CALL(*distiller_page, DistillPageImpl(url, _))
      .WillOnce(DistillerPageOnDistillationDone(distiller_page, url, result));
  return std::unique_ptr<DistillerPage>(distiller_page);
}

std::unique_ptr<DistillerPage> CreateMockDistillerPageWithPendingJSCallback(
    MockDistillerPage** distiller_page_ptr,
    const GURL& url) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  distiller_page->SetMinimumAllowableDistilledContentLength(0);
  *distiller_page_ptr = distiller_page;
  EXPECT_CALL(*distiller_page, DistillPageImpl(url, _));
  return std::unique_ptr<DistillerPage>(distiller_page);
}

TEST_F(DistillerTest, DistillPage) {
  base::Value result = CreateDistilledValueReturnedFromJS(kTitle, kContent);
  distiller_ = std::make_unique<DistillerImpl>(DistillerOptions());
  DistillPage(kURL, CreateMockDistillerPage(&result, GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  ASSERT_EQ(article_proto_->pages_size(), 1);
  const DistilledPageProto& first_page = article_proto_->pages(0);
  EXPECT_EQ(kContent, first_page.html());
  EXPECT_EQ(kURL, first_page.url());
  EXPECT_EQ(kTextContent, first_page.text_content());
  EXPECT_EQ(kTextDirection, first_page.text_direction());
  EXPECT_EQ(2, first_page.word_count());
}

TEST_F(DistillerTest, DistillPageMissingTextDirection) {
  base::Value result = CreateDistilledValueReturnedFromJS(
      kTitle, kContent, kTextContent, /*text_direction=*/std::nullopt);
  distiller_ = std::make_unique<DistillerImpl>(DistillerOptions());
  DistillPage(kURL, CreateMockDistillerPage(&result, GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(article_proto_->pages_size(), 1);
  EXPECT_EQ("auto", article_proto_->pages(0).text_direction());
}

TEST_F(DistillerTest, SinglePageDistillationFailure) {
  // To simulate failure return a null value.
  base::Value null_value;
  distiller_ = std::make_unique<DistillerImpl>(DistillerOptions());
  DistillPage(kURL, CreateMockDistillerPage(&null_value, GURL(kURL)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("", article_proto_->title());
  EXPECT_EQ(0, article_proto_->pages_size());
}

TEST_F(DistillerTest, CancelWithDelayedJSCallback) {
  base::Value distilled_value =
      CreateDistilledValueReturnedFromJS(kTitle, kContent);
  MockDistillerPage* distiller_page = nullptr;
  distiller_ = std::make_unique<DistillerImpl>(DistillerOptions());
  DistillPage(kURL, CreateMockDistillerPageWithPendingJSCallback(
                        &distiller_page, GURL(kURL)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(distiller_page);
  // Post the task to execute javascript and then delete the distiller.
  distiller_page->OnDistillationDone(GURL(kURL), &distilled_value);
  distiller_.reset();

  base::RunLoop().RunUntilIdle();
}

}  // namespace dom_distiller
