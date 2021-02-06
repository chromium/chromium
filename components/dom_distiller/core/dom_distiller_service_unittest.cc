// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace dom_distiller {
namespace test {

namespace {

class FakeViewRequestDelegate : public ViewRequestDelegate {
 public:
  ~FakeViewRequestDelegate() override = default;
  MOCK_METHOD1(OnArticleReady, void(const DistilledArticleProto* proto));
  MOCK_METHOD1(OnArticleUpdated,
               void(ArticleDistillationUpdate article_update));
};

void RunDistillerCallback(FakeDistiller* distiller,
                          std::unique_ptr<DistilledArticleProto> proto) {
  distiller->RunDistillerCallback(std::move(proto));
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<DistilledArticleProto> CreateArticleWithURL(
    const std::string& url) {
  std::unique_ptr<DistilledArticleProto> proto(new DistilledArticleProto);
  DistilledPageProto* page = proto->add_pages();
  page->set_url(url);
  return proto;
}

std::unique_ptr<DistilledArticleProto> CreateDefaultArticle() {
  return CreateArticleWithURL("http://www.example.com/default_article_page1");
}

}  // namespace

class DomDistillerServiceTest : public testing::Test {
 public:
  void SetUp() override {
    distiller_factory_ = new MockDistillerFactory();
    distiller_page_factory_ = new MockDistillerPageFactory();
    service_.reset(new DomDistillerService(
        std::unique_ptr<DistillerFactory>(distiller_factory_),
        std::unique_ptr<DistillerPageFactory>(distiller_page_factory_),
        /* distilled_page_prefs */ nullptr,
        /* distiller_ui_handle */ nullptr));
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    distiller_factory_ = nullptr;
    service_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockDistillerFactory* distiller_factory_;
  MockDistillerPageFactory* distiller_page_factory_;
  std::unique_ptr<DomDistillerService> service_;
};

TEST_F(DomDistillerServiceTest, TestViewUrl) {
  FakeDistiller* distiller = new FakeDistiller(false);
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  FakeViewRequestDelegate viewer_delegate;
  GURL url("http://www.example.com/p1");
  std::unique_ptr<ViewerHandle> handle = service_->ViewUrl(
      &viewer_delegate, service_->CreateDefaultDistillerPage(gfx::Size()), url);

  ASSERT_FALSE(distiller->GetArticleCallback().is_null());
  EXPECT_EQ(url, distiller->GetUrl());

  std::unique_ptr<DistilledArticleProto> proto = CreateDefaultArticle();
  EXPECT_CALL(viewer_delegate, OnArticleReady(proto.get()));

  RunDistillerCallback(distiller, std::move(proto));
}

TEST_F(DomDistillerServiceTest, TestMultipleViewUrl) {
  FakeDistiller* distiller = new FakeDistiller(false);
  FakeDistiller* distiller2 = new FakeDistiller(false);
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller))
      .WillOnce(Return(distiller2));

  FakeViewRequestDelegate viewer_delegate;
  FakeViewRequestDelegate viewer_delegate2;

  GURL url("http://www.example.com/p1");
  GURL url2("http://www.example.com/a/p1");

  std::unique_ptr<ViewerHandle> handle = service_->ViewUrl(
      &viewer_delegate, service_->CreateDefaultDistillerPage(gfx::Size()), url);
  std::unique_ptr<ViewerHandle> handle2 = service_->ViewUrl(
      &viewer_delegate2, service_->CreateDefaultDistillerPage(gfx::Size()),
      url2);

  ASSERT_FALSE(distiller->GetArticleCallback().is_null());
  EXPECT_EQ(url, distiller->GetUrl());

  std::unique_ptr<DistilledArticleProto> proto = CreateDefaultArticle();
  EXPECT_CALL(viewer_delegate, OnArticleReady(proto.get()));

  RunDistillerCallback(distiller, std::move(proto));

  ASSERT_FALSE(distiller2->GetArticleCallback().is_null());
  EXPECT_EQ(url2, distiller2->GetUrl());

  std::unique_ptr<DistilledArticleProto> proto2 = CreateDefaultArticle();
  EXPECT_CALL(viewer_delegate2, OnArticleReady(proto2.get()));

  RunDistillerCallback(distiller2, std::move(proto2));
}

TEST_F(DomDistillerServiceTest, TestViewUrlCancelled) {
  FakeDistiller* distiller = new FakeDistiller(false);
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(Return(distiller));

  bool distiller_destroyed = false;
  EXPECT_CALL(*distiller, Die())
      .WillOnce(testing::Assign(&distiller_destroyed, true));

  FakeViewRequestDelegate viewer_delegate;
  GURL url("http://www.example.com/p1");
  std::unique_ptr<ViewerHandle> handle = service_->ViewUrl(
      &viewer_delegate, service_->CreateDefaultDistillerPage(gfx::Size()), url);

  ASSERT_FALSE(distiller->GetArticleCallback().is_null());
  EXPECT_EQ(url, distiller->GetUrl());

  EXPECT_CALL(viewer_delegate, OnArticleReady(_)).Times(0);

  EXPECT_FALSE(distiller_destroyed);

  handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(distiller_destroyed);
}

}  // namespace test
}  // namespace dom_distiller
