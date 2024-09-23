// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_TEST_MOCK_QUICK_ANSWERS_CLIENT_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_TEST_MOCK_QUICK_ANSWERS_CLIENT_H_

#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace quick_answers {

class MockQuickAnswersClient : public QuickAnswersClient {
 public:
  MockQuickAnswersClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      QuickAnswersDelegate* quick_answers_delegate);
  ~MockQuickAnswersClient() override;

  MOCK_METHOD(void,
              SendRequest,
              (const QuickAnswersRequest& quick_ansers_request),
              (override));
  MOCK_METHOD(void, OnQuickAnswerClick, (ResultType result_type), (override));
  MOCK_METHOD(void,
              OnQuickAnswersDismissed,
              (ResultType result_type, bool is_active),
              (override));
  MOCK_METHOD(void,
              SendRequestForPreprocessing,
              (const QuickAnswersRequest& quick_answers_request),
              (override));
};

}  // namespace quick_answers

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_TEST_MOCK_QUICK_ANSWERS_CLIENT_H_
