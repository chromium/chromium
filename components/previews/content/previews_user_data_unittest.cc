// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_user_data.h"

#include <stdint.h>

#include <memory>

#include "base/message_loop/message_loop.h"
#include "net/base/request_priority.h"
#include "net/nqe/effective_connection_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

class PreviewsUserDataTest : public testing::Test {
 public:
  PreviewsUserDataTest() {}
  ~PreviewsUserDataTest() override {}

 private:
  base::MessageLoopForIO message_loop_;
};

TEST_F(PreviewsUserDataTest, TestConstructor) {
  uint64_t id = 5u;
  std::unique_ptr<PreviewsUserData> data(new PreviewsUserData(5u));
  EXPECT_EQ(id, data->page_id());
}

TEST_F(PreviewsUserDataTest, DeepCopy) {
  uint64_t id = 4u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);
  EXPECT_EQ(id, data->page_id());

  EXPECT_EQ(0, data->data_savings_inflation_percent());
  EXPECT_FALSE(data->cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::NONE, data->committed_previews_type());
  EXPECT_FALSE(data->black_listed_for_lite_page());
  EXPECT_FALSE(data->offline_preview_used());

  data->SetDataSavingsInflationPercent(123);
  data->SetCacheControlNoTransformDirective();
  data->SetCommittedPreviewsType(previews::PreviewsType::NOSCRIPT);
  data->set_offline_preview_used(true);
  data->set_black_listed_for_lite_page(true);

  PreviewsUserData copy(*data);
  EXPECT_EQ(id, copy.page_id());
  EXPECT_EQ(123, copy.data_savings_inflation_percent());
  EXPECT_TRUE(copy.cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT, copy.committed_previews_type());
  EXPECT_TRUE(copy.black_listed_for_lite_page());
  EXPECT_TRUE(copy.offline_preview_used());
}

}  // namespace

}  // namespace previews
