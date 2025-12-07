// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/shared_tab_group_data_specifics_full_proto.pb.h"
#include "components/sync/protocol/sync_collaboration_attribution_full_proto.pb.h"
#include "google/protobuf/descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::SharedTab;
using sync_pb::SharedTabGroup;
using sync_pb::SharedTabGroupDataSpecifics;
using sync_pb::sync_collaboration_attribution;
using sync_pb::SyncCollaborationAttribution;

namespace {
// Test annotations in uncommented full proto buffer.
TEST(SharedTabGroupDataSpecificsFullProtoTest,
     ShouldReflectAndReadCustomAnnotations) {
  const google::protobuf::Descriptor* group_descriptor =
      SharedTabGroup::descriptor();
  ASSERT_TRUE(group_descriptor);

  // Verify the 'title' field annotation.
  const google::protobuf::FieldDescriptor* group_title_field =
      group_descriptor->FindFieldByName("title");
  ASSERT_TRUE(group_title_field);

  const google::protobuf::FieldOptions& group_title_options =
      group_title_field->options();
  EXPECT_TRUE(group_title_options.HasExtension(sync_collaboration_attribution));

  const SyncCollaborationAttribution& group_title_annotation =
      group_title_options.GetExtension(sync_collaboration_attribution);

  EXPECT_EQ(group_title_annotation.attribution_type(),
            SyncCollaborationAttribution::LAST_MODIFIED_BY_USER);
  EXPECT_EQ(group_title_annotation.name(), "tab_group_title");

  // Verify the 'color' field annotation.
  const google::protobuf::FieldDescriptor* group_color_field =
      group_descriptor->FindFieldByName("color");
  ASSERT_TRUE(group_color_field);

  const google::protobuf::FieldOptions& group_color_options =
      group_color_field->options();
  EXPECT_TRUE(group_color_options.HasExtension(sync_collaboration_attribution));

  const SyncCollaborationAttribution& group_color_annotation =
      group_color_options.GetExtension(sync_collaboration_attribution);

  EXPECT_EQ(group_color_annotation.attribution_type(),
            SyncCollaborationAttribution::LAST_MODIFIED_BY_USER);
  EXPECT_EQ(group_color_annotation.name(), "tab_group_color");

  // Test annotations on SharedTab message
  const google::protobuf::Descriptor* tab_descriptor = SharedTab::descriptor();
  ASSERT_TRUE(tab_descriptor);

  // Verify the 'url' field annotation.
  const google::protobuf::FieldDescriptor* tab_url_field =
      tab_descriptor->FindFieldByName("url");
  ASSERT_TRUE(tab_url_field);

  const google::protobuf::FieldOptions& tab_url_options =
      tab_url_field->options();
  EXPECT_TRUE(tab_url_options.HasExtension(sync_collaboration_attribution));

  const SyncCollaborationAttribution& tab_url_annotation =
      tab_url_options.GetExtension(sync_collaboration_attribution);

  EXPECT_EQ(tab_url_annotation.attribution_type(),
            SyncCollaborationAttribution::LAST_MODIFIED_BY_USER);
  EXPECT_EQ(tab_url_annotation.name(), "tab_url");

  // Verify the 'title' field annotation for tab.
  const google::protobuf::FieldDescriptor* tab_title_field =
      tab_descriptor->FindFieldByName("title");
  ASSERT_TRUE(tab_title_field);

  const google::protobuf::FieldOptions& tab_title_options =
      tab_title_field->options();
  EXPECT_TRUE(tab_title_options.HasExtension(sync_collaboration_attribution));

  const SyncCollaborationAttribution& tab_title_annotation =
      tab_title_options.GetExtension(sync_collaboration_attribution);

  EXPECT_EQ(tab_title_annotation.attribution_type(),
            SyncCollaborationAttribution::LAST_MODIFIED_BY_USER);
  EXPECT_EQ(tab_title_annotation.name(), "tab_title");
}

}  // namespace
