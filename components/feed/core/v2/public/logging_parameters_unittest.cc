// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/logging_parameters.h"

#include "components/feed/core/proto/v2/ui.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

TEST(LoggingParameters, ToFromProto_Set) {
  LoggingParameters params;
  params.email = "foo@bar.com";
  params.client_instance_id = "instanceid";
  params.logging_enabled = true;
  params.view_actions_enabled = true;

  feedui::LoggingParameters proto;
  ToProto(params, proto);

  EXPECT_EQ("foo@bar.com", proto.email());
  EXPECT_EQ("instanceid", proto.client_instance_id());
  EXPECT_EQ(true, proto.logging_enabled());
  EXPECT_EQ(true, proto.view_actions_enabled());
  EXPECT_EQ(params, FromProto(proto));
}

TEST(LoggingParameters, ToFromProto_Empty) {
  LoggingParameters params;

  feedui::LoggingParameters proto;
  ToProto(params, proto);

  EXPECT_EQ("", proto.email());
  EXPECT_EQ("", proto.client_instance_id());
  EXPECT_EQ(false, proto.logging_enabled());
  EXPECT_EQ(false, proto.view_actions_enabled());
  EXPECT_EQ(params, FromProto(proto));
}

}  // namespace feed
