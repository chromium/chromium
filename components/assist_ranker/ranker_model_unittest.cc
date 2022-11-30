// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/ranker_model.h"

#include <memory>

#include "base/time/time.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using assist_ranker::RankerModel;

const char kModelURL[] = "https://some.url.net/model";

int64_t InSeconds(const base::Time t) {
  return (t - base::Time()).InSeconds();
}

std::unique_ptr<RankerModel> NewModel(const std::string& model_url,
                                      base::Time last_modified,
                                      base::TimeDelta cache_duration) {
  std::unique_ptr<RankerModel> model = std::make_unique<RankerModel>();
  auto* metadata = model->mutable_proto()->mutable_metadata();
  if (!model_url.empty())
    metadata->set_source(model_url);
  if (!last_modified.is_null())
    metadata->set_last_modified_sec(InSeconds(last_modified));
  if (!cache_duration.is_zero())
    metadata->set_cache_duration_sec(cache_duration.InSeconds());

  auto* translate = model->mutable_proto()->mutable_translate();
  translate->set_version(1);

  auto* logit = translate->mutable_translate_logistic_regression_model();
  logit->set_bias(0.1f);
  logit->set_accept_ratio_weight(0.2f);
  logit->set_decline_ratio_weight(0.3f);
  logit->set_ignore_ratio_weight(0.4f);
  return model;
}

}  // namespace

TEST(RankerModelTest, Serialization) {
  base::Time last_modified = base::Time::Now();
  base::TimeDelta cache_duration = base::Days(3);
  std::unique_ptr<RankerModel> original_model =
      NewModel(kModelURL, last_modified, cache_duration);
  std::string original_model_str = original_model->SerializeAsString();
  std::unique_ptr<RankerModel> serialized_model =
      RankerModel::FromString(original_model_str);
  std::string serialized_model_str = serialized_model->SerializeAsString();

  EXPECT_EQ(serialized_model_str, original_model_str);
  EXPECT_EQ(serialized_model->GetSourceURL(), kModelURL);
  EXPECT_EQ(serialized_model->proto().metadata().last_modified_sec(),
            InSeconds(last_modified));
  EXPECT_EQ(serialized_model->proto().metadata().cache_duration_sec(),
            cache_duration.InSeconds());
}

TEST(RankerModelTest, IsExpired) {
  base::Time today = base::Time::Now();
  base::TimeDelta days_15 = base::Days(15);
  base::TimeDelta days_30 = base::Days(30);
  base::TimeDelta days_60 = base::Days(60);

  EXPECT_FALSE(NewModel(kModelURL, today, days_30)->IsExpired());
  EXPECT_FALSE(NewModel(kModelURL, today - days_15, days_30)->IsExpired());
  EXPECT_TRUE(NewModel(kModelURL, today - days_60, days_30)->IsExpired());
}
