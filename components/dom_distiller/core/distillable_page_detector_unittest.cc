// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distillable_page_detector.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {
namespace {

class Builder {
 public:
  Builder() {}

  Builder& Stump(int feature_number, double split, double weight) {
    StumpProto* stump = proto_.add_stump();
    stump->set_feature_number(feature_number);
    stump->set_split(split);
    stump->set_weight(weight);
    return *this;
  }

  std::unique_ptr<DistillablePageDetector> Build() {
    int num_features = 0;
    for (int i = 0; i < proto_.stump_size(); ++i) {
      num_features =
          std::max(num_features, proto_.stump(i).feature_number() + 1);
    }
    proto_.set_num_features(num_features);
    proto_.set_num_stumps(proto_.stump_size());
    return std::make_unique<DistillablePageDetector>(
        std::make_unique<AdaBoostProto>(proto_));
  }

 private:
  AdaBoostProto proto_;
};

}  // namespace

TEST(DomDistillerDistillablePageDetectorTest, TestCalculateThreshold) {
  std::unique_ptr<DistillablePageDetector> detector =
      Builder().Stump(0, 1.0, 1.0).Stump(0, 1.4, 2.0).Build();

  EXPECT_DOUBLE_EQ(1.5, detector->GetThreshold());

  detector = Builder().Stump(0, 1.0, -1.0).Stump(0, 1.4, 2.0).Build();
  EXPECT_DOUBLE_EQ(0.5, detector->GetThreshold());

  detector = Builder()
                 .Stump(0, 1.0, 1.0)
                 .Stump(0, 1.4, 2.0)
                 .Stump(1, 0.0, 1.0)
                 .Stump(2, 1.0, -3.0)
                 .Build();
  EXPECT_DOUBLE_EQ(0.5, detector->GetThreshold());
}

TEST(DomDistillerDistillablePageDetectorTest, TestScoreAndClassify) {
  std::unique_ptr<DistillablePageDetector> detector =
      Builder().Stump(0, 1.0, 1.0).Stump(0, 1.4, 2.0).Build();
  EXPECT_DOUBLE_EQ(1.5, detector->GetThreshold());

  std::vector<double> features;
  features.push_back(2.0);
  EXPECT_DOUBLE_EQ(3.0, detector->Score(features));
  EXPECT_TRUE(detector->Classify(features));

  features[0] = 1.1;
  EXPECT_DOUBLE_EQ(1.0, detector->Score(features));
  EXPECT_FALSE(detector->Classify(features));

  detector = Builder()
                 .Stump(0, 1.0, 1.0)
                 .Stump(0, -2.0, 2.0)
                 .Stump(1, 0.0, 1.0)
                 .Stump(2, 1.0, -3.0)
                 .Build();
  features.clear();
  features.push_back(-3.0);
  features.push_back(1.0);
  features.push_back(3.0);
  EXPECT_DOUBLE_EQ(-2.0, detector->Score(features));
  EXPECT_FALSE(detector->Classify(features));
  features[2] = 0.0;
  EXPECT_DOUBLE_EQ(1.0, detector->Score(features));
  EXPECT_TRUE(detector->Classify(features));
}

TEST(DomDistillerDistillablePageDetectorTest, TestScoreWrongNumberFeatures) {
  std::unique_ptr<DistillablePageDetector> detector =
      Builder().Stump(0, 1.0, 1.0).Stump(0, 1.4, 2.0).Build();
  EXPECT_DOUBLE_EQ(1.5, detector->GetThreshold());

  std::vector<double> features;
  EXPECT_DOUBLE_EQ(0.0, detector->Score(features));
  features.push_back(-3.0);
  features.push_back(1.0);
  EXPECT_DOUBLE_EQ(0.0, detector->Score(features));
}

}  // namespace dom_distiller
