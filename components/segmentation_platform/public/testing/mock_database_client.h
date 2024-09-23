// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_DATABASE_CLIENT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_DATABASE_CLIENT_H_

#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockDatabaseClient : public DatabaseClient {
 public:
  MockDatabaseClient() = default;
  ~MockDatabaseClient() override = default;

  MOCK_METHOD(void,
              ProcessFeatures,
              (const proto::SegmentationModelMetadata&,
               base::Time,
               FeaturesCallback));
  MOCK_METHOD(void, AddEvent, (const StructuredEvent&));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_TESTING_MOCK_DATABASE_CLIENT_H_
