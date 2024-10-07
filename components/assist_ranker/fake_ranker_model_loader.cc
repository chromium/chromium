// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/fake_ranker_model_loader.h"

namespace assist_ranker {

namespace testing {

FakeRankerModelLoader::FakeRankerModelLoader(
    ValidateModelCallback validate_model_cb,
    OnModelAvailableCallback on_model_available_cb,
    std::unique_ptr<RankerModel> ranker_model)
    : ranker_model_(std::move(ranker_model)),
      validate_model_cb_(std::move(validate_model_cb)),
      on_model_available_cb_(std::move(on_model_available_cb)) {}

FakeRankerModelLoader::~FakeRankerModelLoader() = default;

void FakeRankerModelLoader::NotifyOfRankerActivity() {
  if (validate_model_cb_.Run(*ranker_model_) == RankerModelStatus::OK) {
    on_model_available_cb_.Run(std::move(ranker_model_));
  }
}

}  // namespace testing

}  // namespace assist_ranker
