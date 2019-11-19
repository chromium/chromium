// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/fake_distiller.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {
namespace test {

MockDistillerFactory::MockDistillerFactory() {}
MockDistillerFactory::~MockDistillerFactory() {}

FakeDistiller::FakeDistiller(bool execute_callback)
    : execute_callback_(execute_callback), destruction_allowed_(true) {
  EXPECT_CALL(*this, Die()).Times(testing::AnyNumber());
}

FakeDistiller::FakeDistiller(
    bool execute_callback,
    const base::Closure& distillation_initiated_callback)
    : execute_callback_(execute_callback),
      destruction_allowed_(true),
      distillation_initiated_callback_(distillation_initiated_callback) {
  EXPECT_CALL(*this, Die()).Times(testing::AnyNumber());
}

FakeDistiller::~FakeDistiller() {
  EXPECT_TRUE(destruction_allowed_);
  Die();
}

void FakeDistiller::DistillPage(
    const GURL& url,
    std::unique_ptr<DistillerPage> distiller_page,
    const DistillationFinishedCallback& article_callback,
    const DistillationUpdateCallback& page_callback) {
  url_ = url;
  article_callback_ = article_callback;
  page_callback_ = page_callback;
  if (!distillation_initiated_callback_.is_null()) {
    std::move(distillation_initiated_callback_).Run();
  }
  if (execute_callback_) {
    std::unique_ptr<DistilledArticleProto> proto(new DistilledArticleProto);
    proto->add_pages()->set_url(url_.spec());
    PostDistillerCallback(std::move(proto));
  }
}

void FakeDistiller::RunDistillerCallback(
    std::unique_ptr<DistilledArticleProto> proto) {
  ASSERT_FALSE(execute_callback_) << "Cannot explicitly run the distiller "
                                     "callback for a fake distiller created "
                                     "with automatic callback execution.";
  PostDistillerCallback(std::move(proto));
}

void FakeDistiller::RunDistillerUpdateCallback(
    const ArticleDistillationUpdate& update) {
  page_callback_.Run(update);
}

void FakeDistiller::PostDistillerCallback(
    std::unique_ptr<DistilledArticleProto> proto) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDistiller::RunDistillerCallbackInternal,
                                base::Unretained(this), std::move(proto)));
}

void FakeDistiller::RunDistillerCallbackInternal(
    std::unique_ptr<DistilledArticleProto> proto) {
  EXPECT_FALSE(article_callback_.is_null());

  base::AutoReset<bool> dont_delete_this_in_callback(&destruction_allowed_,
                                                     false);
  article_callback_.Run(std::move(proto));
  article_callback_.Reset();
}

}  // namespace test
}  // namespace dom_distiller
