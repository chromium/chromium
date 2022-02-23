// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "components/autofill_assistant/content/renderer/autofill_assistant_model_executor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"

namespace autofill_assistant {
namespace {

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
  EXPECT_EQ(result->first, 47 /* ADDRESS_LINE1 */);
  EXPECT_EQ(result->second, 7 /* FILL_DELIVERY_ADDRESS */);
}

}  // namespace
}  // namespace autofill_assistant
