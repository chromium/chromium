// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using OnDeviceModelExecutionProtoValueUtilsTest = testing::Test;

TEST(OnDeviceModelExecutionProtoValueUtilsTest, GetStringFromValue) {
  {
    proto::Value value;
    value.set_string_value("hello");
    EXPECT_EQ(GetStringFromValue(value), "hello");
  }

  {
    proto::Value value;
    value.set_boolean_value(true);
    EXPECT_EQ(GetStringFromValue(value), "true");
  }

  {
    proto::Value value;
    value.set_int32_value(123);
    EXPECT_EQ(GetStringFromValue(value), "123");
  }

  {
    proto::Value value;
    value.set_int64_value(12345);
    EXPECT_EQ(GetStringFromValue(value), "12345");
  }

  {
    proto::Value value;
    value.set_float_value(0.5);
    EXPECT_EQ(GetStringFromValue(value), "0.5");
  }
}

}  // namespace
}  // namespace optimization_guide
