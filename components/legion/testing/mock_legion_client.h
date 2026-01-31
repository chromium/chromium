// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_TESTING_MOCK_LEGION_CLIENT_H_
#define COMPONENTS_LEGION_TESTING_MOCK_LEGION_CLIENT_H_

#include "components/legion/client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace legion {

class MockLegionClient : public legion::Client {
 public:
  MOCK_METHOD(void,
              EstablishSession,
              (OnEstablishSessionCompletedCallback callback),
              (override));
  MOCK_METHOD(void,
              SendTextRequest,
              (legion::proto::FeatureName feature_name,
               const std::string& text,
               OnTextRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
  MOCK_METHOD(void,
              SendGenerateContentRequest,
              (legion::proto::FeatureName feature_name,
               const legion::proto::GenerateContentRequest& request,
               OnGenerateContentRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
  MOCK_METHOD(void,
              SendPaicRequest,
              (legion::proto::FeatureName feature_name,
               const legion::proto::PaicMessage& request,
               OnPaicMessageRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_TESTING_MOCK_LEGION_CLIENT_H_
