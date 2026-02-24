// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

#include <array>
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Property;

MATCHER_P4(MatchesFormField,
           id_attribute,
           name_attribute,
           form_control_type,
           value,
           "") {
  return testing::ExplainMatchResult(
             Field("id_attribute", &PageContext::FormField::id_attribute,
                   id_attribute),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             Field("name_attribute", &PageContext::FormField::name_attribute,
                   name_attribute),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             Field("form_control_type",
                   &PageContext::FormField::form_control_type,
                   form_control_type),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             Field("value", &PageContext::FormField::value, value), arg,
             result_listener);
}

MATCHER_P(MatchesPageContext, fields_matcher, "") {
  return testing::ExplainMatchResult(
      Field("fields", &PageContext::FormFieldInfo::fields, fields_matcher),
      arg.form_field_info, result_listener);
}

MATCHER_P6(MatchesEntry,
           guid,
           url,
           title,
           device_name,
           target_device_sync_cache_guid,
           page_context_matcher,
           "") {
  return testing::ExplainMatchResult(
             Property("GUID", &SendTabToSelfEntry::GetGUID, guid), arg,
             result_listener) &&
         testing::ExplainMatchResult(
             Property("URL", &SendTabToSelfEntry::GetURL, url), arg,
             result_listener) &&
         testing::ExplainMatchResult(
             Property("Title", &SendTabToSelfEntry::GetTitle, title), arg,
             result_listener) &&
         testing::ExplainMatchResult(
             Property("DeviceName", &SendTabToSelfEntry::GetDeviceName,
                      device_name),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             Property("TargetDeviceSyncCacheGuid",
                      &SendTabToSelfEntry::GetTargetDeviceSyncCacheGuid,
                      target_device_sync_cache_guid),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             Property("PageContext", &SendTabToSelfEntry::GetPageContext,
                      page_context_matcher),
             arg, result_listener);
}

PageContext::FormField MakeFormField(std::u16string id_attribute,
                                     std::u16string value) {
  PageContext::FormField field;
  field.id_attribute = std::move(id_attribute);
  field.form_control_type = "text";
  field.value = std::move(value);
  return field;
}

TEST(SendTabToSelfEntry, SharedTime) {
  const SendTabToSelfEntry e("1", GURL("http://example.com"), "bar",
                             base::Time::FromTimeT(10), "device", "device2",
                             PageContext());
  EXPECT_EQ("bar", e.GetTitle());
  // Getters return Base::Time values.
  EXPECT_EQ(e.GetSharedTime(), base::Time::FromTimeT(10));
}

// Tests that the send tab to self entry is correctly encoded to
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, AsProto) {
  const SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                                 base::Time::FromTimeT(10), "device", "device2",
                                 PageContext());
  const SendTabToSelfLocal pb_entry = entry.AsLocalProto();
  const sync_pb::SendTabToSelfSpecifics& specifics = pb_entry.specifics();

  EXPECT_EQ(entry.GetGUID(), specifics.guid());
  EXPECT_EQ(entry.GetURL().spec(), specifics.url());
  EXPECT_EQ(entry.GetTitle(), specifics.title());
  EXPECT_EQ(entry.GetDeviceName(), specifics.device_name());
  EXPECT_EQ(entry.GetTargetDeviceSyncCacheGuid(),
            specifics.target_device_sync_cache_guid());
  EXPECT_EQ(entry.GetSharedTime().ToDeltaSinceWindowsEpoch().InMicroseconds(),
            specifics.shared_time_usec());
  EXPECT_FALSE(specifics.has_page_context());
}

// Tests that the send tab to self entry is correctly created from the required
// fields
TEST(SendTabToSelfEntry, FromRequiredFields) {
  EXPECT_THAT(
      SendTabToSelfEntry::FromRequiredFields("1", GURL("http://example.com"),
                                             "target_device"),
      Pointee(MatchesEntry("1", GURL("http://example.com"), "", "",
                           "target_device", MatchesPageContext(IsEmpty()))));
}

// Tests that the send tab to self entry is correctly parsed from
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, FromProto) {
  sync_pb::SendTabToSelfSpecifics pb_entry;
  pb_entry.set_guid("1");
  pb_entry.set_url("http://example.com/");
  pb_entry.set_title("title");
  pb_entry.set_device_name("device");
  pb_entry.set_target_device_sync_cache_guid("device");
  pb_entry.set_shared_time_usec(1);

  EXPECT_THAT(
      SendTabToSelfEntry::FromProto(pb_entry, base::Time::FromTimeT(10)),
      Pointee(MatchesEntry(pb_entry.guid(), GURL(pb_entry.url()),
                           pb_entry.title(), pb_entry.device_name(),
                           pb_entry.target_device_sync_cache_guid(),
                           MatchesPageContext(IsEmpty()))));
}

// Tests that the send tab to self entry expiry works as expected
TEST(SendTabToSelfEntry, IsExpired) {
  const SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                                 base::Time::FromTimeT(10), "device1",
                                 "device1", PageContext());

  EXPECT_TRUE(entry.IsExpired(base::Time::FromTimeT(11) + base::Days(10)));
  EXPECT_FALSE(entry.IsExpired(base::Time::FromTimeT(11)));
}

// Tests that the send tab to self entry rejects strings that are not utf8.
TEST(SendTabToSelfEntry, InvalidStrings) {
  const std::array<char16_t, 1> term = {u'\uFDD1'};
  std::string invalid_utf8;
  base::UTF16ToUTF8(&term[0], 1, &invalid_utf8);

  const SendTabToSelfEntry invalid1("1", GURL("http://example.com"),
                                    invalid_utf8, base::Time::FromTimeT(10),
                                    "device", "device", PageContext());

  EXPECT_EQ("1", invalid1.GetGUID());

  const SendTabToSelfEntry invalid2(invalid_utf8, GURL("http://example.com"),
                                    "title", base::Time::FromTimeT(10),
                                    "device", "device", PageContext());

  EXPECT_EQ(invalid_utf8, invalid2.GetGUID());

  const SendTabToSelfEntry invalid3("1", GURL("http://example.com"), "title",
                                    base::Time::FromTimeT(10), invalid_utf8,
                                    "device", PageContext());

  EXPECT_EQ("1", invalid3.GetGUID());

  const SendTabToSelfEntry invalid4("1", GURL("http://example.com"), "title",
                                    base::Time::FromTimeT(10), "device",
                                    invalid_utf8, PageContext());

  EXPECT_EQ("1", invalid4.GetGUID());

  sync_pb::SendTabToSelfSpecifics pb_entry;
  pb_entry.set_guid(invalid_utf8);
  pb_entry.set_url("http://example.com/");
  pb_entry.set_title(invalid_utf8);
  pb_entry.set_device_name(invalid_utf8);
  pb_entry.set_target_device_sync_cache_guid("device");
  pb_entry.set_shared_time_usec(1);

  EXPECT_THAT(
      SendTabToSelfEntry::FromProto(pb_entry, base::Time::FromTimeT(10)),
      Pointee(Property(&SendTabToSelfEntry::GetGUID, invalid_utf8)));
}

// Tests that the send tab to self entry is correctly encoded to
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, MarkAsOpened) {
  SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                           base::Time::FromTimeT(10), "device", "device2",
                           PageContext());
  EXPECT_FALSE(entry.IsOpened());
  entry.MarkOpened();
  EXPECT_TRUE(entry.IsOpened());

  sync_pb::SendTabToSelfSpecifics pb_entry;
  pb_entry.set_guid("1");
  pb_entry.set_url("http://example.com/");
  pb_entry.set_title("title");
  pb_entry.set_device_name("device");
  pb_entry.set_target_device_sync_cache_guid("device");
  pb_entry.set_shared_time_usec(1);
  pb_entry.set_opened(true);

  EXPECT_THAT(
      SendTabToSelfEntry::FromProto(pb_entry, base::Time::FromTimeT(10)),
      Pointee(Property(&SendTabToSelfEntry::IsOpened, true)));
}

TEST(SendTabToSelfEntry, PageContextRoundTrip) {
  PageContext context;
  context.form_field_info.fields.push_back(MakeFormField(u"id1", u"value1"));

  const SendTabToSelfEntry entry("1", GURL("http://example.com"), "title",
                                 base::Time::FromTimeT(10), "device", "device2",
                                 context);

  const SendTabToSelfLocal local_proto = entry.AsLocalProto();

  EXPECT_THAT(
      SendTabToSelfEntry::FromProto(local_proto.specifics(),
                                    base::Time::FromTimeT(10)),
      Pointee(MatchesEntry(_, _, _, _, _,
                           MatchesPageContext(ElementsAre(
                               MatchesFormField(u"id1", _, _, u"value1"))))));
}

TEST(SendTabToSelfEntry, PageContextSizeLimit) {
  PageContext context;
  // Add a field with a very large value to exceed 6 KB.
  context.form_field_info.fields.push_back(
      MakeFormField(u"id1", std::u16string(kMaxPageContextSizeBytes, u'a')));

  const SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                                 base::Time::FromTimeT(10), "device", "device2",
                                 context);

  const SendTabToSelfLocal pb_entry = entry.AsLocalProto();
  // The page context should be cleared because it exceeds the limit.
  EXPECT_FALSE(pb_entry.specifics().has_page_context());
}

TEST(SendTabToSelfEntry, TextFragment) {
  TextFragmentData tf_data;
  tf_data.text_start = "start";
  tf_data.text_end = "end";
  tf_data.prefix = "prefix";
  tf_data.suffix = "suffix";

  PageContext context;
  context.scroll_position.text_fragment = tf_data;

  SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                           base::Time::FromTimeT(10), "device", "device2",
                           context);

  EXPECT_EQ(tf_data, entry.GetPageContext().scroll_position.text_fragment);

  SendTabToSelfLocal local_pb = entry.AsLocalProto();
  EXPECT_TRUE(local_pb.specifics().has_page_context());
  EXPECT_TRUE(local_pb.specifics().page_context().has_scroll_position());
  EXPECT_TRUE(local_pb.specifics()
                  .page_context()
                  .scroll_position()
                  .has_text_fragment());
  EXPECT_EQ("start", local_pb.specifics()
                         .page_context()
                         .scroll_position()
                         .text_fragment()
                         .text_start());

  std::unique_ptr<SendTabToSelfEntry> entry2 =
      SendTabToSelfEntry::FromProto(local_pb.specifics(), base::Time::Now());
  EXPECT_EQ(tf_data, entry2->GetPageContext().scroll_position.text_fragment);
}

}  // namespace

}  // namespace send_tab_to_self
