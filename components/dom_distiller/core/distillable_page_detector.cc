// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distillable_page_detector.h"

#include <stddef.h>
#include <utility>

#include "base/check.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {

const DistillablePageDetector* DistillablePageDetector::GetNewModel() {
  static DistillablePageDetector* detector = nullptr;
  if (!detector) {
    std::string serialized_proto =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_DISTILLABLE_PAGE_SERIALIZED_MODEL_NEW);
    std::unique_ptr<AdaBoostProto> proto(new AdaBoostProto);
    CHECK(proto->ParseFromString(serialized_proto));
    detector = new DistillablePageDetector(std::move(proto));
  }
  return detector;
}

const DistillablePageDetector* DistillablePageDetector::GetLongPageModel() {
  static DistillablePageDetector* detector = nullptr;
  if (!detector) {
    std::string serialized_proto =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_LONG_PAGE_SERIALIZED_MODEL);
    std::unique_ptr<AdaBoostProto> proto(new AdaBoostProto);
    CHECK(proto->ParseFromString(serialized_proto));
    detector = new DistillablePageDetector(std::move(proto));
  }
  return detector;
}

DistillablePageDetector::DistillablePageDetector(
    std::unique_ptr<AdaBoostProto> proto)
    : proto_(std::move(proto)), threshold_(0.0) {
  CHECK(proto_->num_stumps() == proto_->stump_size());
  for (int i = 0; i < proto_->num_stumps(); ++i) {
    const StumpProto& stump = proto_->stump(i);
    CHECK(stump.feature_number() >= 0);
    CHECK(stump.feature_number() < proto_->num_features());
    threshold_ += stump.weight() / 2.0;
  }
}

DistillablePageDetector::~DistillablePageDetector() = default;

bool DistillablePageDetector::Classify(
    const std::vector<double>& features) const {
  return Score(features) > threshold_;
}

double DistillablePageDetector::Score(
    const std::vector<double>& features) const {
  if (features.size() != size_t(proto_->num_features())) {
    return 0.0;
  }
  double score = 0.0;
  for (int i = 0; i < proto_->num_stumps(); ++i) {
    const StumpProto& stump = proto_->stump(i);
    if (features[stump.feature_number()] > stump.split()) {
      score += stump.weight();
    }
  }
  return score;
}

double DistillablePageDetector::GetThreshold() const {
  return threshold_;
}

}  // namespace dom_distiller
