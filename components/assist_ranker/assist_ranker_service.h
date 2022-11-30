// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_ASSIST_RANKER_SERVICE_H_
#define COMPONENTS_ASSIST_RANKER_ASSIST_RANKER_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace assist_ranker {

class BinaryClassifierPredictor;
struct PredictorConfig;

// Service that provides Predictor objects.
class AssistRankerService : public KeyedService {
 public:
  AssistRankerService() = default;

  AssistRankerService(const AssistRankerService&) = delete;
  AssistRankerService& operator=(const AssistRankerService&) = delete;

  // Returns a binary classification model given a PredictorConfig.
  // The predictor is instantiated the first time a predictor is fetched. The
  // next calls to fetch will return a pointer to the already instantiated
  // predictor.
  virtual base::WeakPtr<BinaryClassifierPredictor>
  FetchBinaryClassifierPredictor(const PredictorConfig& config) = 0;
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_ASSIST_RANKER_SERVICE_H_
