// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_TEST_HELPERS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_TEST_HELPERS_H_

#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/result_loader.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/components/quick_answers/utils/unit_conversion_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace quick_answers {

std::string GetQuickAnswerTextForTesting(
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements);

// Build a dict representing a unit, given the provided fields.
base::Value::Dict CreateUnit(const std::string& name,
                             double rate_a = kInvalidRateTermValue,
                             double rate_b = kInvalidRateTermValue,
                             const std::string& category = std::string(),
                             double rate_c = kInvalidRateTermValue);

class MockQuickAnswersDelegate : public QuickAnswersDelegate {
 public:
  MockQuickAnswersDelegate();
  ~MockQuickAnswersDelegate() override;

  MockQuickAnswersDelegate(const MockQuickAnswersDelegate&) = delete;
  MockQuickAnswersDelegate& operator=(const MockQuickAnswersDelegate&) = delete;

  // QuickAnswersClient::QuickAnswersDelegate:
  MOCK_METHOD1(OnQuickAnswerReceived,
               void(std::unique_ptr<QuickAnswersSession>));
  MOCK_METHOD1(OnRequestPreprocessFinished, void(const QuickAnswersRequest&));
  MOCK_METHOD0(OnNetworkError, void());
};

class MockResultLoaderDelegate : public ResultLoader::ResultLoaderDelegate {
 public:
  MockResultLoaderDelegate();

  MockResultLoaderDelegate(const MockResultLoaderDelegate&) = delete;
  MockResultLoaderDelegate& operator=(const MockResultLoaderDelegate&) = delete;

  ~MockResultLoaderDelegate() override;

  // ResultLoader::ResultLoaderDelegate:
  MOCK_METHOD0(OnNetworkError, void());
  MOCK_METHOD1(OnQuickAnswerReceived,
               void(std::unique_ptr<QuickAnswersSession>));
};

MATCHER_P(QuickAnswerEqual, quick_answer, "") {
  return (GetQuickAnswerTextForTesting(arg->first_answer_row) ==
              GetQuickAnswerTextForTesting(quick_answer->first_answer_row) &&
          GetQuickAnswerTextForTesting(arg->title) ==
              GetQuickAnswerTextForTesting(quick_answer->title));
}

MATCHER_P(QuickAnswersRequestEqual, quick_answers_request, "") {
  return (arg.selected_text == quick_answers_request.selected_text);
}

MATCHER_P(PreprocessedOutputEqual, preprocessed_output, "") {
  return (arg.query == preprocessed_output.query);
}

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_TEST_TEST_HELPERS_H_
