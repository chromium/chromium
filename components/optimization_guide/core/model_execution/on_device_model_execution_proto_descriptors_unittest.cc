// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"

#include "components/optimization_guide/proto/features/compose.pb.h"
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

}  // namespace

}  // namespace optimization_guide
