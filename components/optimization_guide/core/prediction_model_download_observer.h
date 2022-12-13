// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_OBSERVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Observes the PredictionModelDownloadManager for downloads that have
// completed.
class PredictionModelDownloadObserver : public base::CheckedObserver {
 public:
  // Invoked when a model has been downloaded and verified. |base_model_dir| is
  // the base dir where model files are available.
  virtual void OnModelReady(const base::FilePath& base_model_dir,
                            const proto::PredictionModel& model) = 0;

  // Invoked when a model download started.
  virtual void OnModelDownloadStarted(
      proto::OptimizationTarget optimization_target) = 0;

  // Invoked when a model download, its extraction, verification, etc., failed.
  virtual void OnModelDownloadFailed(
      proto::OptimizationTarget optimization_target) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_OBSERVER_H_
