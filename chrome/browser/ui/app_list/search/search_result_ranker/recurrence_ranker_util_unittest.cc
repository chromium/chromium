// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_util.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::FloatEq;

namespace app_list {

class RecurrenceRankerJsonConfigConverterTest : public testing::Test {
 public:
  // Converts the async JsonConfigConverter::Convert call into a synchronous
  // call for ease of testing.
  base::Optional<RecurrenceRankerConfigProto> Convert(const std::string& json) {
    base::RunLoop run_loop;
    done_callback_ = run_loop.QuitClosure();
    converter_ = JsonConfigConverter::Convert(
        json, "",
        base::BindOnce(
            [](RecurrenceRankerJsonConfigConverterTest* fixture,
               base::Optional<RecurrenceRankerConfigProto> config) {
              fixture->converter_.reset();
              fixture->config_ = config;
              fixture->done_callback_.Run();
            },
            base::Unretained(this)));
    run_loop.Run();
    return config_;
  }

  // A convenience wrapper of Convert that CHECKs if the conversion fails.
  RecurrenceRankerConfigProto ConvertAndAssertValid(const std::string& json) {
    const auto& converted_proto = Convert(json);
    CHECK(converted_proto.has_value());
    return converted_proto.value();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  std::unique_ptr<JsonConfigConverter> converter_;

  base::Closure done_callback_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::Optional<RecurrenceRankerConfigProto> config_;
};

TEST_F(RecurrenceRankerJsonConfigConverterTest, ParseFailures) {
  // Empty.
  EXPECT_FALSE(Convert(""));

  // Not valid JSON.
  EXPECT_FALSE(Convert("{"));

  // Missing field (target_limit).
  EXPECT_FALSE(Convert(
      R"({
        "min_seconds_between_saves": 250,
        "target_decay": 0.5,
        "condition_limit": 50,
        "condition_decay": 0.7,
        "predictor": {
          "predictor_type": "fake"
        }
      })"));

  // Invalid field type (target_limit).
  EXPECT_FALSE(Convert(
      R"({
        "min_seconds_between_saves": 250,
        "target_limit": 100.5,
        "target_decay": 0.5,
        "condition_limit": 50,
        "condition_decay": 0.7,
        "predictor": {
          "predictor_type": "fake"
        }
      })"));

  // Missing predictor.
  EXPECT_FALSE(Convert(
      R"({
        "min_seconds_between_saves": 250,
        "target_limit": 100,
        "target_decay": 0.5,
        "condition_limit": 50,
        "condition_decay": 0.7,
      })"));

  // Invalid predictor type string.
  EXPECT_FALSE(Convert(
      R"({
        "min_seconds_between_saves": 250,
        "target_limit": 100,
        "target_decay": 0.5,
        "condition_limit": 50,
        "condition_decay": 0.7,
        "predictor": {
          "predictor_type": "doesn't exist"
        }
      })"));

  // Hour binned predictor with no parameters.
  EXPECT_FALSE(Convert(
      R"({
        "min_seconds_between_saves": 250,
        "target_limit": 100,
        "target_decay": 0.5,
        "condition_limit": 50,
        "condition_decay": 0.7,
        "predictor": {
          "predictor_type": "hour binned"
        }
      })"));

  // Predictor type mismatched with predictor config.
  EXPECT_FALSE(Convert(
      R"({
        "min_seconds_between_saves": 250,
        "target_limit": 100,
        "target_decay": 0.5,
        "condition_limit": 50,
        "condition_decay": 0.7,
        "predictor": {
          "predictor_type": "hour binned",
          "frecency_predictor": {
            "decay_coeff": 0.1
          }
        }
      })"));

  // Frecency predictor with incorrect param.
  EXPECT_FALSE(Convert(
      R"({
        "min_seconds_between_saves": 250,
        "target_limit": 100,
        "target_decay": 0.5,
        "condition_limit": 50,
        "condition_decay": 0.7,
        "predictor": {
          "predictor_type": "frecency",
          "frecency": {
            "not a param": 1.0
          }
        }
      })"));
}

// Tests that the top-level fields are properly propagated. Following tests
// check the parameters of individual fields.
TEST_F(RecurrenceRankerJsonConfigConverterTest, RankerFieldsPropagated) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type":"fake"
      }
    })";
  const auto& proto = ConvertAndAssertValid(json);

  EXPECT_EQ(proto.min_seconds_between_saves(), 250u);
  EXPECT_EQ(proto.target_limit(), 100u);
  EXPECT_EQ(proto.target_decay(), 0.5f);
  EXPECT_EQ(proto.condition_limit(), 50u);
  EXPECT_EQ(proto.condition_decay(), 0.7f);
  EXPECT_TRUE(proto.predictor().has_fake_predictor());
}

TEST_F(RecurrenceRankerJsonConfigConverterTest, FrecencyPredictorPropagated) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "frecency",
        "decay_coeff": 0.25
      }
    })";
  const auto& proto = ConvertAndAssertValid(json);

  ASSERT_TRUE(proto.predictor().has_frecency_predictor());
  const auto& predictor = proto.predictor().frecency_predictor();

  EXPECT_EQ(predictor.decay_coeff(), 0.25f);
}

TEST_F(RecurrenceRankerJsonConfigConverterTest, DefaultPredictorPropagated) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "default"
      }
    })";
  const auto& proto = ConvertAndAssertValid(json);

  EXPECT_TRUE(proto.predictor().has_default_predictor());
}

TEST_F(RecurrenceRankerJsonConfigConverterTest, HourBinPredictorPropagated) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "hour bin",
        "bin_weights": [
          {"bin":-1, "weight":0.2},
          {"bin":0, "weight":0.3},
          {"bin":4, "weight":0.4}
        ]
      }
    })";
  const auto& proto = ConvertAndAssertValid(json);

  ASSERT_TRUE(proto.predictor().has_hour_bin_predictor());
  const auto& predictor = proto.predictor().hour_bin_predictor();

  EXPECT_EQ(predictor.bin_weights(0).bin(), -1);
  EXPECT_EQ(predictor.bin_weights(0).weight(), 0.2f);
  EXPECT_EQ(predictor.bin_weights(1).bin(), 0);
  EXPECT_EQ(predictor.bin_weights(1).weight(), 0.3f);
  EXPECT_EQ(predictor.bin_weights(2).bin(), 4);
  EXPECT_EQ(predictor.bin_weights(2).weight(), 0.4f);
}

TEST_F(RecurrenceRankerJsonConfigConverterTest,
       ConditionalFrequencyPredictorPropagated) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "conditional frequency"
      }
    })";
  const auto& proto = ConvertAndAssertValid(json);

  EXPECT_TRUE(proto.predictor().has_conditional_frequency_predictor());
}

TEST_F(RecurrenceRankerJsonConfigConverterTest, MarkovPredictorPropagated) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "markov"
      }
    })";
  const auto& proto = ConvertAndAssertValid(json);

  EXPECT_TRUE(proto.predictor().has_markov_predictor());
}

TEST_F(RecurrenceRankerJsonConfigConverterTest,
       ExponentialWeightsEnsemblePropagated) {
  const std::string json = R"({
      "min_seconds_between_saves": 250,
      "target_limit": 100,
      "target_decay": 0.5,
      "condition_limit": 50,
      "condition_decay": 0.7,
      "predictor": {
        "predictor_type": "exponential weights ensemble",
        "learning_rate": 1.6,
        "predictors": [
          {"predictor_type": "default"},
          {"predictor_type": "markov"},
          {"predictor_type": "frecency", "decay_coeff": 0.8}
        ]
      }
    })";
  const auto& proto = ConvertAndAssertValid(json);

  ASSERT_TRUE(proto.predictor().has_exponential_weights_ensemble());

  const auto& predictors =
      proto.predictor().exponential_weights_ensemble().predictors();
  EXPECT_TRUE(predictors.Get(0).has_default_predictor());
  EXPECT_TRUE(predictors.Get(1).has_markov_predictor());
  EXPECT_TRUE(predictors.Get(2).has_frecency_predictor());
}

}  // namespace app_list
