// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_loader.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {
namespace {

constexpr char kValidResponse[] = R"()]}'
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
          }
        }
      }
    ]
  }
)";

}  // namespace

class SearchResultLoaderTest : public testing::Test {
 public:
  SearchResultLoaderTest() = default;

  SearchResultLoaderTest(const SearchResultLoaderTest&) = delete;
  SearchResultLoaderTest& operator=(const SearchResultLoaderTest&) = delete;

  // testing::Test:
  void SetUp() override {
    mock_delegate_ = std::make_unique<MockResultLoaderDelegate>();
    loader_ = std::make_unique<SearchResultLoader>(&test_url_loader_factory_,
                                                   mock_delegate_.get());
  }

  void TearDown() override { loader_.reset(); }

 protected:
  std::unique_ptr<SearchResultLoader> loader_;
  std::unique_ptr<MockResultLoaderDelegate> mock_delegate_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(SearchResultLoaderTest, Success) {
  std::unique_ptr<QuickAnswer> expected_quick_answer =
      std::make_unique<QuickAnswer>();
  expected_quick_answer->primary_answer = "9.055 inches";
  test_url_loader_factory_.AddResponse(assistant::kSampleKnowledgeApiRequest,
                                       kValidResponse);
  EXPECT_CALL(
      *mock_delegate_,
      OnQuickAnswerReceived(QuickAnswerEqual(&(*expected_quick_answer))));
  EXPECT_CALL(*mock_delegate_, OnNetworkError()).Times(0);
  loader_->Fetch(PreprocessRequest(IntentInfo("23cm", IntentType::kUnknown)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SearchResultLoaderTest, NetworkError) {
  test_url_loader_factory_.AddResponse(
      GURL(assistant::kSampleKnowledgeApiRequest),
      network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  EXPECT_CALL(*mock_delegate_, OnNetworkError());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(testing::_)).Times(0);
  loader_->Fetch(PreprocessRequest(IntentInfo("23cm", IntentType::kUnknown)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(SearchResultLoaderTest, EmptyResponse) {
  test_url_loader_factory_.AddResponse(assistant::kSampleKnowledgeApiRequest,
                                       std::string());
  EXPECT_CALL(*mock_delegate_, OnQuickAnswerReceived(testing::Eq(nullptr)));
  EXPECT_CALL(*mock_delegate_, OnNetworkError()).Times(0);
  loader_->Fetch(PreprocessRequest(IntentInfo("23cm", IntentType::kUnknown)));
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_answers
}  // namespace chromeos
