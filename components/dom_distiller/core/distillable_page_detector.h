// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLABLE_PAGE_DETECTOR_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLABLE_PAGE_DETECTOR_H_

#include <memory>
#include <vector>

#include "components/dom_distiller/core/proto/adaboost.pb.h"

namespace dom_distiller {

// DistillablePageDetector provides methods to identify whether or not a page is
// likely to be distillable based on a vector of derived features (see
// dom_distiller::CalculateDerivedFeatures). It uses a simple AdaBoost-trained
// model.
class DistillablePageDetector {
 public:
  static const DistillablePageDetector* GetNewModel();
  static const DistillablePageDetector* GetLongPageModel();
  explicit DistillablePageDetector(std::unique_ptr<AdaBoostProto> proto);
  ~DistillablePageDetector();

  // Returns true if the model classifies the vector of features as a
  // distillable page.
  bool Classify(const std::vector<double>& features) const;

  double Score(const std::vector<double>& features) const;
  double GetThreshold() const;

  DistillablePageDetector(const DistillablePageDetector&) = delete;
  DistillablePageDetector& operator=(const DistillablePageDetector) = delete;

 private:
  std::unique_ptr<AdaBoostProto> proto_;
  double threshold_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLABLE_PAGE_DETECTOR_H_
