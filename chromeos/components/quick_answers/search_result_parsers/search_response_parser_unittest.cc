// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {

class SearchResponseParserTest : public testing::Test {
 public:
  SearchResponseParserTest() = default;

  SearchResponseParserTest(const SearchResponseParserTest&) = delete;
  SearchResponseParserTest& operator=(const SearchResponseParserTest&) = delete;

  void SetUp() override {
    search_result_parser_ = std::make_unique<SearchResponseParser>(
        base::BindOnce(&SearchResponseParserTest::SearchResponseParserCallback,
                       base::Unretained(this)));
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void SearchResponseParserCallback(
      std::unique_ptr<QuickAnswersSession> quick_answers_session) {
    if (quick_answers_session) {
      quick_answer_ = std::move(quick_answers_session->quick_answer);
    } else {
      quick_answer_ = nullptr;
    }
    run_loop_->Quit();
  }

  void WaitForResponse() { run_loop_->Run(); }

 protected:
  std::unique_ptr<SearchResponseParser> search_result_parser_;
  std::unique_ptr<QuickAnswer> quick_answer_;
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(SearchResponseParserTest, ProcessResponseSuccessFirstResult) {
  constexpr char kSearchResponse[] = R"()]}'
    {
      "results": [
        {
          "oneNamespaceType": 13668,
          "unitConversionResult": {
            "source": {
              "valueAndUnit": {
                "rawText": "23 centimeters"
              }
            },
            "destination": {
              "valueAndUnit": {
                "rawText": "9.055 inches"
              }
            },
            "category": "Length",
            "sourceAmount": 23
          }
        }
      ]
    }
  )";
  search_result_parser_->ProcessResponse(
      std::make_unique<std::string>(kSearchResponse));
  WaitForResponse();
  EXPECT_TRUE(quick_answer_);
  EXPECT_EQ("9.055 inches",
            GetQuickAnswerTextForTesting(quick_answer_->first_answer_row));
}

TEST_F(SearchResponseParserTest, ProcessResponseSuccessMultipleResults) {
  constexpr char kSearchResponse[] = R"()]}'
    {
      "results": [
        { "oneNamespaceType": 13666 },
        { "oneNamespaceType": 13667 },
        {
          "oneNamespaceType": 13668,
          "unitConversionResult": {
            "source": {
              "valueAndUnit": {
                "rawText": "23 centimeters"
              }
            },
            "destination": {
              "valueAndUnit": {
                "rawText": "9.055 inches"
              }
            },
            "category": "Length",
            "sourceAmount": 23
          }
        }
      ]
    }
  )";
  search_result_parser_->ProcessResponse(
      std::make_unique<std::string>(kSearchResponse));
  WaitForResponse();
  EXPECT_TRUE(quick_answer_);
  EXPECT_EQ("9.055 inches",
            GetQuickAnswerTextForTesting(quick_answer_->first_answer_row));
}

TEST_F(SearchResponseParserTest, ProcessResponseNoResults) {
  // The empty line between the response body and XSSI prefix is intentional to
  // keep it consistent with the actual response we got from the server.
  constexpr char kSearchResponse[] = R"()]}'

    {}
  )";
  search_result_parser_->ProcessResponse(
      std::make_unique<std::string>(kSearchResponse));
  WaitForResponse();
  EXPECT_EQ(nullptr, quick_answer_);
}

TEST_F(SearchResponseParserTest, ProcessResponseEmptyResults) {
  constexpr char kSearchResponse[] = R"()]}'

    { "results": [] }
  )";
  search_result_parser_->ProcessResponse(
      std::make_unique<std::string>(kSearchResponse));
  WaitForResponse();
  EXPECT_EQ(nullptr, quick_answer_);
}

TEST_F(SearchResponseParserTest, ProcessResponseInvalidResponse) {
  search_result_parser_->ProcessResponse(
      std::make_unique<std::string>("results {}"));
  WaitForResponse();
  EXPECT_FALSE(quick_answer_);
}

TEST_F(SearchResponseParserTest, ProcessResponseInvalidXssiPrefix) {
  constexpr char kSearchResponse[] = R"()]'

    {}
  )";
  search_result_parser_->ProcessResponse(
      std::make_unique<std::string>(kSearchResponse));
  WaitForResponse();
  EXPECT_FALSE(quick_answer_);
}

}  // namespace quick_answers
