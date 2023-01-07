// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_CUP_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_CUP_H_

#include "components/autofill_assistant/browser/service/cup.h"
#include "components/autofill_assistant/browser/service/cup_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace cup {

class MockCUP : public CUP {
 public:
  MockCUP();
  ~MockCUP() override;

  MOCK_METHOD1(PackAndSignRequest,
               std::string(const std::string& original_request));

  MOCK_METHOD1(
      UnpackResponse,
      absl::optional<std::string>(const std::string& original_response));
};

class MockCUPFactory : public CUPFactory {
 public:
  MockCUPFactory();
  ~MockCUPFactory() override;

  MOCK_CONST_METHOD1(CreateInstance, std::unique_ptr<CUP>(RpcType rpc_type));
};

}  // namespace cup

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_CUP_H_
