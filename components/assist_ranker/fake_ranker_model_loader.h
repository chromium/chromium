// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_FAKE_RANKER_MODEL_LOADER_H_
#define COMPONENTS_ASSIST_RANKER_FAKE_RANKER_MODEL_LOADER_H_

#include <memory>

#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/ranker_model_loader.h"

namespace assist_ranker {

namespace testing {

// Simplified RankerModelLoader for testing.
class FakeRankerModelLoader : public RankerModelLoader {
 public:
  FakeRankerModelLoader(ValidateModelCallback validate_model_cb,
                        OnModelAvailableCallback on_model_available_cb,
                        std::unique_ptr<RankerModel> ranker_model);

  FakeRankerModelLoader(const FakeRankerModelLoader&) = delete;
  FakeRankerModelLoader& operator=(const FakeRankerModelLoader&) = delete;

  ~FakeRankerModelLoader() override;

  void NotifyOfRankerActivity() override;

 private:
  std::unique_ptr<RankerModel> ranker_model_;
  const ValidateModelCallback validate_model_cb_;
  const OnModelAvailableCallback on_model_available_cb_;
};

}  // namespace testing

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_FAKE_RANKER_MODEL_LOADER_H_
