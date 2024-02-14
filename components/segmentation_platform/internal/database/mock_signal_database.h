// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_SIGNAL_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_SIGNAL_DATABASE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

// A mock of the SignalDatabase.
class MockSignalDatabase : public SignalDatabase {
 public:
  MockSignalDatabase();
  ~MockSignalDatabase() override;

  MOCK_METHOD(void, Initialize, (SignalDatabase::SuccessCallback), (override));
  MOCK_METHOD(void,
              WriteSample,
              (proto::SignalType,
               uint64_t,
               std::optional<int32_t>,
               SignalDatabase::SuccessCallback),
              (override));
  MOCK_METHOD(void,
              GetSamples,
              (proto::SignalType,
               uint64_t,
               base::Time,
               base::Time,
               SignalDatabase::EntriesCallback),
              (override));
  MOCK_METHOD(const std::vector<SignalDatabase::DbEntry>*,
              GetAllSamples,
              (),
              (override));
  MOCK_METHOD(void,
              DeleteSamples,
              (proto::SignalType,
               uint64_t,
               base::Time,
               SignalDatabase::SuccessCallback),
              (override));
  MOCK_METHOD(void,
              CompactSamplesForDay,
              (proto::SignalType,
               uint64_t,
               base::Time,
               SignalDatabase::SuccessCallback),
              (override));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_SIGNAL_DATABASE_H_
