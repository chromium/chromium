// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_MOCK_PRIVATE_AI_CLIENT_H_
#define COMPONENTS_PRIVATE_AI_TESTING_MOCK_PRIVATE_AI_CLIENT_H_

#include "components/private_ai/client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace private_ai {

class MockPrivateAiClient : public Client {
 public:
  MOCK_METHOD(void, EstablishConnection, (), (override));
  MOCK_METHOD(void,
              SendTextRequest,
              (proto::FeatureName feature_name,
               const std::string& text,
               OnTextRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
  MOCK_METHOD(void,
              SendGenerateContentRequest,
              (proto::FeatureName feature_name,
               const proto::GenerateContentRequest& request,
               OnGenerateContentRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
  MOCK_METHOD(void,
              SendPaicRequest,
              (proto::FeatureName feature_name,
               const proto::PaicMessage& request,
               OnPaicMessageRequestCompletedCallback callback,
               const RequestOptions& options),
              (override));
  MOCK_METHOD(PrivateAiLogger*, GetLogger, (), (override));
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_MOCK_PRIVATE_AI_CLIENT_H_
