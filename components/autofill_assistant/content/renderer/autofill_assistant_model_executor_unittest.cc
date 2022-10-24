// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_model_executor.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"

namespace autofill_assistant {
namespace {

using OverridesMap = AutofillAssistantModelExecutor::OverridesMap;
using SparseVector = AutofillAssistantModelExecutor::SparseVector;

constexpr int kDummyObjective = 9999;
constexpr int kDummySemanticRole = 1111;

class AutofillAssistantModelExecutorTest : public testing::Test {
 public:
  AutofillAssistantModelExecutorTest() {
    base::FilePath model_file_path =
        GetTestDataDir().AppendASCII("model.tflite");
    model_file_ = base::File(model_file_path,
                             (base::File::FLAG_OPEN | base::File::FLAG_READ));
  }

  ~AutofillAssistantModelExecutorTest() override = default;

 protected:
  OverridesMap CreateOverrides() {
    OverridesMap map;
    SparseVector vector;
    // First, create a feature vector of the feature "street" that is found
    // twice on the website for the "second" feature index.
    vector.push_back(std::make_pair(
        std::make_pair(/* feature_index = */ 2, /* feature= */ 862),
        /* count= */ 2));
    // Add an override with a dummy objetive and semantic role for that feature
    // vector.
    map[vector] = std::make_pair(kDummyObjective, kDummySemanticRole);
    return map;
  }

  base::File model_file_;
  AutofillAssistantModelExecutor model_executor_;

 private:
  base::FilePath GetTestDataDir() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    return source_root_dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("autofill_assistant")
        .AppendASCII("model");
  }
};

TEST_F(AutofillAssistantModelExecutorTest, DoesNotInitializeFromEmptyFile) {
  EXPECT_FALSE(model_executor_.InitializeModelFromFile(base::File()));
}

TEST_F(AutofillAssistantModelExecutorTest, OnlyInitializesModelOnce) {
  EXPECT_TRUE(model_executor_.InitializeModelFromFile(model_file_.Duplicate()));
  EXPECT_FALSE(
      model_executor_.InitializeModelFromFile(model_file_.Duplicate()));
}

TEST_F(AutofillAssistantModelExecutorTest, ExecuteWithLoadedModel) {
  ASSERT_TRUE(model_executor_.InitializeModelFromFile(model_file_.Duplicate()));

  blink::AutofillAssistantNodeSignals node_signals;
  node_signals.node_features.html_tag = blink::WebString::FromUTF8("INPUT");
  node_signals.node_features.type = blink::WebString::FromUTF8("TEXT");
  node_signals.node_features.text.push_back(
      blink::WebString::FromUTF8("street"));
  node_signals.label_features.text.push_back(
      blink::WebString::FromUTF8("Street Address:"));
  node_signals.label_features.text.push_back(
      blink::WebString::FromUTF8("Line 1"));
  node_signals.context_features.header_text.push_back(
      blink::WebString::FromUTF8("Street Address"));
  node_signals.context_features.header_text.push_back(
      blink::WebString::FromUTF8("Checkout"));
  node_signals.context_features.header_text.push_back(
      blink::WebString::FromUTF8("SHIPPING"));

  auto result = model_executor_.ExecuteModelWithInput(node_signals);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->role, 47 /* ADDRESS_LINE1 */);
  EXPECT_EQ(result->objective, 7 /* FILL_DELIVERY_ADDRESS */);
}

TEST_F(AutofillAssistantModelExecutorTest, OverridesMatch) {
  AutofillAssistantModelExecutor model_executor =
      AutofillAssistantModelExecutor(CreateOverrides());

  ASSERT_TRUE(model_executor.InitializeModelFromFile(model_file_.Duplicate()));

  blink::AutofillAssistantNodeSignals node_signals;
  node_signals.node_features.invisible_attributes =
      blink::WebString::FromUTF8("street");
  node_signals.node_features.text.push_back(
      blink::WebString::FromUTF8("street"));

  auto result = model_executor.ExecuteModelWithInput(node_signals);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->role, 9999);
  EXPECT_EQ(result->objective, 1111);
  EXPECT_TRUE(result->used_override);
}

TEST_F(AutofillAssistantModelExecutorTest, OverridesNoMatch) {
  AutofillAssistantModelExecutor model_executor =
      AutofillAssistantModelExecutor(CreateOverrides());

  ASSERT_TRUE(model_executor.InitializeModelFromFile(model_file_.Duplicate()));

  blink::AutofillAssistantNodeSignals node_signals;
  node_signals.node_features.text.push_back(
      blink::WebString::FromUTF8("street"));

  auto result = model_executor.ExecuteModelWithInput(node_signals);
  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->role, 9999);
  EXPECT_NE(result->objective, 1111);
  EXPECT_FALSE(result->used_override);
}

TEST_F(AutofillAssistantModelExecutorTest, OverridesResultNotReused) {
  AutofillAssistantModelExecutor model_executor =
      AutofillAssistantModelExecutor(CreateOverrides());

  ASSERT_TRUE(model_executor.InitializeModelFromFile(model_file_.Duplicate()));
  {
    blink::AutofillAssistantNodeSignals node_signals;
    node_signals.node_features.invisible_attributes =
        blink::WebString::FromUTF8("street");
    node_signals.node_features.text.push_back(
        blink::WebString::FromUTF8("street"));

    auto result = model_executor.ExecuteModelWithInput(node_signals);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->role, 9999);
    EXPECT_EQ(result->objective, 1111);
    EXPECT_TRUE(result->used_override);
  }

  // We expect the internal overrides result from the previous execution to have
  // been cleared.
  {
    blink::AutofillAssistantNodeSignals node_signals;
    node_signals.node_features.text.push_back(
        blink::WebString::FromUTF8("unknown"));

    auto result = model_executor.ExecuteModelWithInput(node_signals);
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->role, 9999);
    EXPECT_NE(result->objective, 1111);
    EXPECT_FALSE(result->used_override);
  }
}

}  // namespace
}  // namespace autofill_assistant
