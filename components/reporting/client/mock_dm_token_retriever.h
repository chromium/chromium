// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_MOCK_DM_TOKEN_RETRIEVER_H_
#define COMPONENTS_REPORTING_CLIENT_MOCK_DM_TOKEN_RETRIEVER_H_

#include <cstddef>
#include <string>

#include "components/reporting/client/dm_token_retriever.h"
#include "components/reporting/util/statusor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace reporting {

// A mock |DMTokenRetriever| that stubs out functionality that retrieves the
// DM token for testing purposes.
class MockDMTokenRetriever : public DMTokenRetriever {
 public:
  MockDMTokenRetriever();
  MockDMTokenRetriever(const MockDMTokenRetriever&) = delete;
  MockDMTokenRetriever& operator=(const MockDMTokenRetriever&) = delete;
  ~MockDMTokenRetriever() override;

  // Ensures by mocking that RetrieveDMToken is expected to be triggered a
  // specific number of times and runs the completion callback with the
  // specified result on trigger.
  void ExpectRetrieveDMTokenAndReturnResult(
      size_t times,
      const StatusOr<std::string> dm_token_result);

  // Mocked stub that retrieves the DM token and triggers the specified callback
  MOCK_METHOD(void,
              RetrieveDMToken,
              (DMTokenRetriever::CompletionCallback completion_cb),
              (override));
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_MOCK_DM_TOKEN_RETRIEVER_H_
