// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"

#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

TEST(ProtoDescriptorsTest, SetStringFieldOk) {
  proto::ComposeRequest req;
  EXPECT_EQ(SetProtoField(req.mutable_generate_params(), 1, "foo"),
            ProtoStatus::kOk);
  EXPECT_EQ(req.mutable_generate_params()->user_input(), "foo");
}

TEST(ProtoDescriptorsTest, SetStringFieldError) {
  proto::ComposeRequest req;
  EXPECT_EQ(SetProtoField(req.mutable_generate_params(), 999, "foo"),
            ProtoStatus::kError);
}

TEST(ProtoDescriptorsTest, MutableMessageField) {
  proto::ComposeRequest req;
  EXPECT_EQ(SetProtoField(GetProtoMutableMessage(&req, 7), 1, "foo"),
            ProtoStatus::kOk);
  EXPECT_EQ(req.mutable_generate_params()->user_input(), "foo");
}

TEST(ProtoDescriptorsTest, MutableMessageError) {
  proto::ComposeRequest req;
  EXPECT_EQ(GetProtoMutableMessage(&req, 999), nullptr);
}

TEST(ProtoDescriptorsTest, MutableRepeatedMessageField) {
  proto::TabOrganizationRequest req;
  EXPECT_EQ(AddProtoMessage(&req, 1), 1);
  SetProtoField(GetProtoMutableRepeatedMessage(&req, 1, 0), 2, "mytitle");
  EXPECT_EQ(req.tabs(0).title(), "mytitle");
}

TEST(ProtoDescriptorsTest, MutableRepeatedMessageError) {
  proto::TabOrganizationRequest req;
  EXPECT_EQ(AddProtoMessage(&req, 999), 0);
  EXPECT_EQ(GetProtoMutableRepeatedMessage(&req, 1, 0), nullptr);
}

}  // namespace

}  // namespace optimization_guide
