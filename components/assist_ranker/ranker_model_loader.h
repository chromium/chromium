// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_RANKER_MODEL_LOADER_H_
#define COMPONENTS_ASSIST_RANKER_RANKER_MODEL_LOADER_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/assist_ranker/ranker_model.h"

namespace assist_ranker {

// Enumeration denoting the outcome of an attempt to download the model. This
// must be kept in sync with the RankerModelStatus enum in histograms.xml
enum class RankerModelStatus {
  OK = 0,
  DOWNLOAD_THROTTLED = 1,
  DOWNLOAD_FAILED = 2,
  PARSE_FAILED = 3,
  VALIDATION_FAILED = 4,
  INCOMPATIBLE = 5,
  LOAD_FROM_CACHE_FAILED = 6,
  MODEL_LOADING_ABANDONED = 7,

  // Insert new values above this line.
  MAX
};

// Loads a ranker model. Will attempt to load the model from disk cache. If it
// fails, will attempt to download from the given URL.
class RankerModelLoader {
 public:
  // Callback to validate a ranker model on behalf of the model loader client.
  // For example, the callback might validate that the model is compatible with
  // the features generated when ranking translation offerings.  This will be
  // called on the sequence on which the model loader was constructed.
  using ValidateModelCallback = base::RepeatingCallback<RankerModelStatus(
      const assist_ranker::RankerModel&)>;

  // Called to transfer ownership of a loaded model back to the model loader
  // client. This will be called on the sequence on which the model loader was
  // constructed.
  using OnModelAvailableCallback = base::RepeatingCallback<void(
      std::unique_ptr<assist_ranker::RankerModel>)>;

  RankerModelLoader() = default;

  RankerModelLoader(const RankerModelLoader&) = delete;
  RankerModelLoader& operator=(const RankerModelLoader&) = delete;

  virtual ~RankerModelLoader() = default;
  // Call this method periodically to notify the model loader the ranker is
  // actively in use. The user's engagement with the ranked feature is used
  // as a proxy for network availability and activity. If a model download
  // is pending, this will trigger (subject to retry and frequency limits) a
  // model download attempt.
  virtual void NotifyOfRankerActivity() = 0;
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_RANKER_MODEL_LOADER_H_
