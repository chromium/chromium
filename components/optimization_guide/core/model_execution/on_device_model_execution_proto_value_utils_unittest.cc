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

TEST(OnDeviceModelExecutionProtoValueUtilsTest, AreValuesEqualCaseDifferent) {
  proto::Value a;
  a.set_string_value("hello");

  proto::Value b;
  b.set_int32_value(123);

  EXPECT_FALSE(AreValuesEqual(a, b));
}

TEST(OnDeviceModelExecutionProtoValueUtilsTest, AreValuesEqualStrings) {
  {
    proto::Value a;
    a.set_string_value("hello");

    proto::Value b;
    b.set_string_value("whatever");

    EXPECT_FALSE(AreValuesEqual(a, b));
  }

  {
    proto::Value a;
    a.set_string_value("hello");

    proto::Value b;
    b.set_string_value("hello");

    EXPECT_TRUE(AreValuesEqual(a, b));
  }
}

TEST(OnDeviceModelExecutionProtoValueUtilsTest, AreValuesEqualBool) {
  {
    proto::Value a;
    a.set_boolean_value(true);

    proto::Value b;
    b.set_boolean_value(false);

    EXPECT_FALSE(AreValuesEqual(a, b));
  }

  {
    proto::Value a;
    a.set_boolean_value(false);

    proto::Value b;
    b.set_boolean_value(false);

    EXPECT_TRUE(AreValuesEqual(a, b));
  }
}

TEST(OnDeviceModelExecutionProtoValueUtilsTest, AreValuesEqualInt32) {
  {
    proto::Value a;
    a.set_int32_value(1);

    proto::Value b;
    b.set_int32_value(123);

    EXPECT_FALSE(AreValuesEqual(a, b));
  }

  {
    proto::Value a;
    a.set_int32_value(123);

    proto::Value b;
    b.set_int32_value(123);

    EXPECT_TRUE(AreValuesEqual(a, b));
  }
}

TEST(OnDeviceModelExecutionProtoValueUtilsTest, AreValuesEqualInt64) {
  {
    proto::Value a;
    a.set_int64_value(3);

    proto::Value b;
    b.set_int64_value(123);

    EXPECT_FALSE(AreValuesEqual(a, b));
  }

  {
    proto::Value a;
    a.set_int64_value(123);

    proto::Value b;
    b.set_int64_value(123);

    EXPECT_TRUE(AreValuesEqual(a, b));
  }
}

TEST(OnDeviceModelExecutionProtoValueUtilsTest, AreValuesEqualFloat) {
  {
    proto::Value a;
    a.set_float_value(0.1);

    proto::Value b;
    b.set_float_value(0.2);

    EXPECT_FALSE(AreValuesEqual(a, b));
  }

  {
    proto::Value a;
    a.set_float_value(0.2);

    proto::Value b;
    b.set_float_value(0.2);

    EXPECT_TRUE(AreValuesEqual(a, b));
  }
}

}  // namespace
}  // namespace optimization_guide
