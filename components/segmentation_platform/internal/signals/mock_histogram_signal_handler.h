// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_MOCK_HISTOGRAM_SIGNAL_HANDLER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_MOCK_HISTOGRAM_SIGNAL_HANDLER_H_

#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"

#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockHistogramSignalHandler : public HistogramSignalHandler {
 public:
  MockHistogramSignalHandler();
  ~MockHistogramSignalHandler() override;

  MOCK_METHOD(void, SetRelevantHistograms, (const RelevantHistograms&));
  MOCK_METHOD(void, EnableMetrics, (bool));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SIGNALS_MOCK_HISTOGRAM_SIGNAL_HANDLER_H_
