// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_SIGNAL_STORAGE_CONFIG_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_SIGNAL_STORAGE_CONFIG_H_

#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

// A mock of the SignalStorageConfig.
class MockSignalStorageConfig : public SignalStorageConfig {
 public:
  using SignalType = proto::SignalType;
  using SignalIdentifier = std::pair<uint64_t, SignalType>;

  MockSignalStorageConfig();
  ~MockSignalStorageConfig() override;

  MOCK_METHOD(bool,
              MeetsSignalCollectionRequirement,
              (const proto::SegmentationModelMetadata& model_metadata, bool),
              (override));

  MOCK_METHOD(void,
              OnSignalCollectionStarted,
              (const proto::SegmentationModelMetadata& model_metadata),
              (override));

  MOCK_METHOD(void,
              GetSignalsForCleanup,
              (const std::set<SignalIdentifier>& known_signals,
               std::vector<CleanupItem>& result),
              (const override));

  MOCK_METHOD(void,
              UpdateSignalsForCleanup,
              (const std::vector<CleanupItem>& signals),
              (override));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_SIGNAL_STORAGE_CONFIG_H_
