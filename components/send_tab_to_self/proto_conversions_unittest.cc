// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/proto_conversions.h"

#include <optional>

#include "components/autofill/core/browser/field_types.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {
namespace {

// Helper to create a sync_pb::FormField with the given attributes.
sync_pb::FormField CreateProtoFormField(const std::string& id,
                                        const std::string& name,
                                        const std::string& control_type,
                                        const std::string& value) {
  sync_pb::FormField field;
  field.set_id_attribute(id);
  field.set_name_attribute(name);
  field.set_form_control_type(control_type);
  field.set_value(value);
  return field;
}

TEST(SendTabToSelfProtoConversionsTest, AutofillFieldTypeToProto_FillableType) {
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::EMAIL_ADDRESS),
            sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS);
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::USERNAME),
            sync_pb::FormField_AutofillFieldType_USERNAME);
}

TEST(SendTabToSelfProtoConversionsTest,
     AutofillFieldTypeToProto_NonFillableType) {
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::UNKNOWN_TYPE), std::nullopt);
  EXPECT_EQ(AutofillFieldTypeToProto(autofill::MERCHANT_EMAIL_SIGNUP),
            std::nullopt);
}

// Tests that PageContextFromProto skips form fields with an invalid
// form control type, ensuring only valid types are processed.
TEST(SendTabToSelfProtoConversionsTest,
     PageContextFromProto_InvalidControlType) {
  sync_pb::PageContext pb_context;
  *pb_context.mutable_form_field_info()->add_fields() =
      CreateProtoFormField("id", "name", "invalid_type", "value");

  PageContext context =
      PageContextFromProto(pb_context, GURL("https://example.com/page"));
  // The invalid field should have been skipped.
  EXPECT_TRUE(context.form_field_info.fields.empty());
}

TEST(SendTabToSelfProtoConversionsTest, PageContextToProto_PopulatesUrl) {
  PageContext context;
  context.scroll_position.text_fragment.text_start = "fragment";

  sync_pb::PageContext pb_context =
      PageContextToProto(context, GURL("https://example.com/page"));
  EXPECT_EQ(pb_context.url(), "https://example.com/page");
}

TEST(SendTabToSelfProtoConversionsTest,
     PageContextToProto_EmptyContextRemainsEmpty) {
  sync_pb::PageContext pb_context =
      PageContextToProto(PageContext(), GURL("https://example.com/page"));
  EXPECT_FALSE(pb_context.has_url());
  EXPECT_EQ(pb_context.ByteSizeLong(), 0u);
}

TEST(SendTabToSelfProtoConversionsTest, PageContextFromProto_MatchingUrl) {
  sync_pb::PageContext pb_context;
  pb_context.set_url("https://example.com/page");
  pb_context.mutable_scroll_position()->mutable_text_fragment()->set_text_start(
      "fragment");

  PageContext context =
      PageContextFromProto(pb_context, GURL("https://example.com/page"));
  EXPECT_EQ(context.scroll_position.text_fragment.text_start, "fragment");
}

TEST(SendTabToSelfProtoConversionsTest, PageContextFromProto_MismatchedUrl) {
  sync_pb::PageContext pb_context;
  pb_context.set_url("https://example.com/page");
  pb_context.mutable_scroll_position()->mutable_text_fragment()->set_text_start(
      "fragment");

  PageContext context =
      PageContextFromProto(pb_context, GURL("https://other.example.com/page"));
  EXPECT_TRUE(context.scroll_position.IsEmpty());
}

TEST(SendTabToSelfProtoConversionsTest, PageContextFromProto_MissingUrl) {
  sync_pb::PageContext pb_context;
  pb_context.mutable_scroll_position()->mutable_text_fragment()->set_text_start(
      "fragment");

  PageContext context =
      PageContextFromProto(pb_context, GURL("https://example.com/page"));
  EXPECT_EQ(context.scroll_position.text_fragment.text_start, "fragment");
}

}  // namespace
}  // namespace send_tab_to_self
