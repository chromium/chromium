// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_ON_DEVICE_TAIL_MODEL_SERVICE_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_ON_DEVICE_TAIL_MODEL_SERVICE_H_

#include "components/omnibox/browser/on_device_tail_model_service.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/proto/models.pb.h"

// Fake implementation of on device tail model service for use with tests.
class FakeOnDeviceTailModelService : public OnDeviceTailModelService {
 public:
  FakeOnDeviceTailModelService();

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_ON_DEVICE_TAIL_MODEL_SERVICE_H_
