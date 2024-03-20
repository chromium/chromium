// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"

#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructTextSafetyRequestNoSafetyFallbackConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("whatever");
  EXPECT_EQ(std::nullopt, adapter->ConstructTextSafetyRequest(request, "text"));
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructTextSafetyRequestNoUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.mutable_text_safety_fallback_config();
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("whatever");
  auto safety_request = adapter->ConstructTextSafetyRequest(request, "text");

  ASSERT_TRUE(safety_request);
  EXPECT_EQ("text", safety_request->text());
  EXPECT_TRUE(safety_request->url().empty());
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructTextSafetyRequestWithUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* ts_config = config.mutable_text_safety_fallback_config();
  auto* input_url_proto_field = ts_config->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(3);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(1);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  auto safety_request = adapter->ConstructTextSafetyRequest(request, "text");

  ASSERT_TRUE(safety_request);
  EXPECT_EQ("text", safety_request->text());
  EXPECT_EQ("url", safety_request->url());
}

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructTextSafetyRequestWithBadUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* ts_config = config.mutable_text_safety_fallback_config();
  auto* input_url_proto_field = ts_config->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(100);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(100);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  EXPECT_EQ(std::nullopt, adapter->ConstructTextSafetyRequest(request, "text"));
}

}  // namespace optimization_guide
