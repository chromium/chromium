// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_value_conversions.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/protocol/contact_info_specifics.pb.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/managed_user_setting_specifics.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/os_preference_specifics.pb.h"
#include "components/sync/protocol/os_priority_preference_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/priority_preference_specifics.pb.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sharing_message_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::Not;

// Keep this file in sync with the .proto files in this directory.

#define DEFINE_SPECIFICS_TO_VALUE_TEST(Key)                         \
  TEST(ProtoValueConversionsTest, Proto_##Key##_SpecificsToValue) { \
    sync_pb::EntitySpecifics specifics;                             \
    specifics.mutable_##Key();                                      \
    base::Value value = EntitySpecificsToValue(specifics);          \
    ASSERT_TRUE(value.is_dict());                                   \
    EXPECT_EQ(1u, value.GetDict().size());                          \
  }

// We'd also like to check if we changed any field in our messages. However,
// that's hard to do: sizeof could work, but it's platform-dependent.
// default_instance().ByteSize() won't change for most changes, since most of
// our fields are optional. So we just settle for comments in the proto files.

DEFINE_SPECIFICS_TO_VALUE_TEST(encrypted)

static_assert(53 == syncer::GetNumDataTypes(),
              "When adding a new field, add a DEFINE_SPECIFICS_TO_VALUE_TEST "
              "for your field below, and optionally a test for the specific "
              "conversions.");

DEFINE_SPECIFICS_TO_VALUE_TEST(app)
DEFINE_SPECIFICS_TO_VALUE_TEST(app_list)
DEFINE_SPECIFICS_TO_VALUE_TEST(app_setting)
DEFINE_SPECIFICS_TO_VALUE_TEST(arc_package)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_offer)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_profile)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_wallet)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_wallet_credential)
DEFINE_SPECIFICS_TO_VALUE_TEST(autofill_wallet_usage)
DEFINE_SPECIFICS_TO_VALUE_TEST(bookmark)
DEFINE_SPECIFICS_TO_VALUE_TEST(collaboration_group)
DEFINE_SPECIFICS_TO_VALUE_TEST(contact_info)
DEFINE_SPECIFICS_TO_VALUE_TEST(cookie)
DEFINE_SPECIFICS_TO_VALUE_TEST(device_info)
DEFINE_SPECIFICS_TO_VALUE_TEST(dictionary)
DEFINE_SPECIFICS_TO_VALUE_TEST(extension)
DEFINE_SPECIFICS_TO_VALUE_TEST(extension_setting)
DEFINE_SPECIFICS_TO_VALUE_TEST(history)
DEFINE_SPECIFICS_TO_VALUE_TEST(history_delete_directive)
DEFINE_SPECIFICS_TO_VALUE_TEST(incoming_password_sharing_invitation)
DEFINE_SPECIFICS_TO_VALUE_TEST(managed_user_setting)
DEFINE_SPECIFICS_TO_VALUE_TEST(nigori)
DEFINE_SPECIFICS_TO_VALUE_TEST(os_preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(os_priority_preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(outgoing_password_sharing_invitation)
DEFINE_SPECIFICS_TO_VALUE_TEST(password)
DEFINE_SPECIFICS_TO_VALUE_TEST(plus_address)
DEFINE_SPECIFICS_TO_VALUE_TEST(plus_address_setting)
DEFINE_SPECIFICS_TO_VALUE_TEST(power_bookmark)
DEFINE_SPECIFICS_TO_VALUE_TEST(preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(printer)
DEFINE_SPECIFICS_TO_VALUE_TEST(printers_authorization_server)
DEFINE_SPECIFICS_TO_VALUE_TEST(priority_preference)
DEFINE_SPECIFICS_TO_VALUE_TEST(product_comparison)
DEFINE_SPECIFICS_TO_VALUE_TEST(reading_list)
DEFINE_SPECIFICS_TO_VALUE_TEST(saved_tab_group)
DEFINE_SPECIFICS_TO_VALUE_TEST(search_engine)
DEFINE_SPECIFICS_TO_VALUE_TEST(security_event)
DEFINE_SPECIFICS_TO_VALUE_TEST(send_tab_to_self)
DEFINE_SPECIFICS_TO_VALUE_TEST(session)
DEFINE_SPECIFICS_TO_VALUE_TEST(shared_tab_group_data)
DEFINE_SPECIFICS_TO_VALUE_TEST(sharing_message)
DEFINE_SPECIFICS_TO_VALUE_TEST(theme)
DEFINE_SPECIFICS_TO_VALUE_TEST(typed_url)
DEFINE_SPECIFICS_TO_VALUE_TEST(user_consent)
DEFINE_SPECIFICS_TO_VALUE_TEST(user_event)
DEFINE_SPECIFICS_TO_VALUE_TEST(wallet_metadata)
DEFINE_SPECIFICS_TO_VALUE_TEST(web_apk)
DEFINE_SPECIFICS_TO_VALUE_TEST(web_app)
DEFINE_SPECIFICS_TO_VALUE_TEST(webauthn_credential)
DEFINE_SPECIFICS_TO_VALUE_TEST(wifi_configuration)
DEFINE_SPECIFICS_TO_VALUE_TEST(workspace_desk)

TEST(ProtoValueConversionsTest, AutofillWalletSpecificsToValue) {
  sync_pb::AutofillWalletSpecifics specifics;
  specifics.mutable_masked_card()->set_name_on_card("Igloo");
  specifics.mutable_address()->set_recipient_name("John");
  specifics.mutable_customer_data()->set_id("123456");
  specifics.mutable_cloud_token_data()->set_masked_card_id("1111");

  specifics.set_type(sync_pb::AutofillWalletSpecifics::UNKNOWN);
  base::Value::Dict value =
      AutofillWalletSpecificsToValue(specifics).TakeDict();
  EXPECT_FALSE(value.contains("masked_card"));
  EXPECT_FALSE(value.contains("address"));
  EXPECT_FALSE(value.contains("customer_data"));
  EXPECT_FALSE(value.contains("cloud_token_data"));

  specifics.set_type(sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);
  value = AutofillWalletSpecificsToValue(specifics).TakeDict();
  EXPECT_TRUE(value.contains("masked_card"));
  EXPECT_FALSE(value.contains("address"));
  EXPECT_FALSE(value.contains("customer_data"));
  EXPECT_FALSE(value.contains("cloud_token_data"));

  specifics.set_type(sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS);
  value = AutofillWalletSpecificsToValue(specifics).TakeDict();
  EXPECT_FALSE(value.contains("masked_card"));
  EXPECT_TRUE(value.contains("address"));
  EXPECT_FALSE(value.contains("customer_data"));
  EXPECT_FALSE(value.contains("cloud_token_data"));

  specifics.set_type(sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA);
  value = AutofillWalletSpecificsToValue(specifics).TakeDict();
  EXPECT_FALSE(value.contains("masked_card"));
  EXPECT_FALSE(value.contains("address"));
  EXPECT_TRUE(value.contains("customer_data"));
  EXPECT_FALSE(value.contains("cloud_token_data"));

  specifics.set_type(
      sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA);
  value = AutofillWalletSpecificsToValue(specifics).TakeDict();
  EXPECT_FALSE(value.contains("masked_card"));
  EXPECT_FALSE(value.contains("address"));
  EXPECT_FALSE(value.contains("customer_data"));
  EXPECT_TRUE(value.contains("cloud_token_data"));
}

TEST(ProtoValueConversionsTest, BookmarkSpecificsData) {
  const base::Time creation_time(base::Time::Now());
  const std::string icon_url = "http://www.google.com/favicon.ico";
  sync_pb::BookmarkSpecifics specifics;
  specifics.set_creation_time_us(creation_time.ToInternalValue());
  specifics.set_icon_url(icon_url);
  sync_pb::MetaInfo* meta_1 = specifics.add_meta_info();
  meta_1->set_key("key1");
  meta_1->set_value("value1");
  sync_pb::MetaInfo* meta_2 = specifics.add_meta_info();
  meta_2->set_key("key2");
  meta_2->set_value("value2");

  base::Value::Dict value = BookmarkSpecificsToValue(specifics).TakeDict();
  EXPECT_FALSE(value.empty());
  const std::string* encoded_time = value.FindString("creation_time_us");
  EXPECT_TRUE(encoded_time);
  EXPECT_EQ(base::NumberToString(creation_time.ToInternalValue()),
            *encoded_time);
  const std::string* encoded_icon_url = value.FindString("icon_url");
  EXPECT_TRUE(encoded_icon_url);
  EXPECT_EQ(icon_url, *encoded_icon_url);

  const base::Value::List* meta_info_list = value.FindList("meta_info");

  EXPECT_EQ(2u, meta_info_list->size());
  std::string meta_key;
  std::string meta_value;
  const auto& meta_info_value = (*meta_info_list)[0].GetDict();
  ASSERT_TRUE((*meta_info_list)[0].is_dict());
  EXPECT_STREQ("key1", meta_info_value.FindString("key")->c_str());
  EXPECT_STREQ("value1", meta_info_value.FindString("value")->c_str());
  const auto& meta_info_value_1 = (*meta_info_list)[1].GetDict();
  ASSERT_TRUE((*meta_info_list)[1].is_dict());
  EXPECT_STREQ("key2", meta_info_value_1.FindString("key")->c_str());
  EXPECT_STREQ("value2", meta_info_value_1.FindString("value")->c_str());
}

TEST(ProtoValueConversionsTest, UniquePositionToValue) {
  sync_pb::SyncEntity entity;
  entity.mutable_unique_position()->set_custom_compressed_v1("test");

  base::Value::Dict value =
      SyncEntityToValue(entity, {.include_specifics = false}).TakeDict();
  const std::string* unique_position = value.FindString("unique_position");
  EXPECT_TRUE(unique_position);

  std::string expected_unique_position =
      UniquePosition::FromProto(entity.unique_position()).ToDebugString();
  EXPECT_EQ(expected_unique_position, *unique_position);
}

TEST(ProtoValueConversionsTest, SyncEntityToValueIncludeSpecifics) {
  sync_pb::SyncEntity entity;
  entity.mutable_specifics();

  base::Value::Dict value =
      SyncEntityToValue(entity, {.include_specifics = true}).TakeDict();
  EXPECT_TRUE(value.FindDict("specifics"));

  value = SyncEntityToValue(entity, {.include_specifics = false}).TakeDict();
  EXPECT_FALSE(value.FindDict("specifics"));
}

namespace {
// Returns whether the given value has specifics under the entries in the given
// path.
bool ValueHasSpecifics(const base::Value::Dict& value,
                       const std::string& path) {
  const base::Value::List* entities_list = value.FindListByDottedPath(path);
  if (!entities_list) {
    return false;
  }

  const base::Value& entry_dictionary_value = (*entities_list)[0];
  if (!entry_dictionary_value.is_dict()) {
    return false;
  }

  const base::Value::Dict& entry_dictionary = entry_dictionary_value.GetDict();
  return entry_dictionary.FindDict("specifics") != nullptr;
}

MATCHER(ValueHasNonEmptyGetUpdateTriggers, "") {
  const base::Value::Dict& value_dict = arg;

  const base::Value::List* entities_list =
      value_dict.FindListByDottedPath("get_updates.from_progress_marker");
  if (!entities_list) {
    *result_listener << "no from_progress_marker list";
    return false;
  }

  const base::Value& entry_dictionary_value = entities_list->front();
  if (!entry_dictionary_value.is_dict()) {
    *result_listener << "from_progress_marker does not contain a dictionary";
    return false;
  }

  const base::Value::Dict& entry_dictionary = entry_dictionary_value.GetDict();
  const base::Value::Dict* get_update_triggers_dictionary =
      entry_dictionary.FindDict("get_update_triggers");
  if (!get_update_triggers_dictionary) {
    *result_listener << "no get_update_triggers dictionary";
    return false;
  }

  return !get_update_triggers_dictionary->empty();
}
}  // namespace

// Create a ClientToServerMessage with an EntitySpecifics.  Converting it to
// a value should respect the |include_specifics| flag.
TEST(ProtoValueConversionsTest, ClientToServerMessageToValue) {
  sync_pb::ClientToServerMessage message;
  sync_pb::CommitMessage* commit_message = message.mutable_commit();
  sync_pb::SyncEntity* entity = commit_message->add_entries();
  entity->mutable_specifics();

  base::Value::Dict value_with_specifics =
      ClientToServerMessageToValue(message, {.include_specifics = true})
          .TakeDict();
  EXPECT_FALSE(value_with_specifics.empty());
  EXPECT_TRUE(ValueHasSpecifics(value_with_specifics, "commit.entries"));

  base::Value::Dict value_without_specifics =
      ClientToServerMessageToValue(message, {.include_specifics = false})
          .TakeDict();
  EXPECT_FALSE(value_without_specifics.empty());
  EXPECT_FALSE(ValueHasSpecifics(value_without_specifics, "commit.entries"));
}

TEST(ProtoValueConversionsTest, ClientToServerMessageToValueGUTriggers) {
  sync_pb::ClientToServerMessage message;
  sync_pb::GetUpdateTriggers* get_update_triggers =
      message.mutable_get_updates()
          ->add_from_progress_marker()
          ->mutable_get_update_triggers();
  get_update_triggers->set_client_dropped_hints(false);
  get_update_triggers->set_server_dropped_hints(false);
  get_update_triggers->set_datatype_refresh_nudges(0);
  get_update_triggers->set_local_modification_nudges(0);
  get_update_triggers->set_initial_sync_in_progress(false);
  get_update_triggers->set_sync_for_resolve_conflict_in_progress(false);

  base::Value::Dict value_with_full_gu_triggers =
      ClientToServerMessageToValue(message,
                                   {.include_full_get_update_triggers = true})
          .TakeDict();
  EXPECT_FALSE(value_with_full_gu_triggers.empty());
  EXPECT_THAT(value_with_full_gu_triggers, ValueHasNonEmptyGetUpdateTriggers());

  base::Value::Dict value_without_full_gu_triggers =
      ClientToServerMessageToValue(message,
                                   {.include_full_get_update_triggers = false})
          .TakeDict();
  EXPECT_FALSE(value_without_full_gu_triggers.empty());
  EXPECT_THAT(value_without_full_gu_triggers,
              Not(ValueHasNonEmptyGetUpdateTriggers()));
}

// Create a ClientToServerResponse with an EntitySpecifics.  Converting it to
// a value should respect the |include_specifics| flag.
TEST(ProtoValueConversionsTest, ClientToServerResponseToValue) {
  sync_pb::ClientToServerResponse message;
  sync_pb::GetUpdatesResponse* response = message.mutable_get_updates();
  sync_pb::SyncEntity* entity = response->add_entries();
  entity->mutable_specifics();

  base::Value::Dict value_with_specifics =
      ClientToServerResponseToValue(message, {.include_specifics = true})
          .TakeDict();
  EXPECT_FALSE(value_with_specifics.empty());
  EXPECT_TRUE(ValueHasSpecifics(value_with_specifics, "get_updates.entries"));

  base::Value::Dict value_without_specifics =
      ClientToServerResponseToValue(message, {.include_specifics = false})
          .TakeDict();
  EXPECT_FALSE(value_without_specifics.empty());
  EXPECT_FALSE(
      ValueHasSpecifics(value_without_specifics, "get_updates.entries"));
}

TEST(ProtoValueConversionsTest, CompareSpecificsData) {
  sync_pb::ProductComparisonSpecifics specifics;
  specifics.set_uuid("my_uuid");
  specifics.set_creation_time_unix_epoch_millis(1708532099);
  specifics.set_update_time_unix_epoch_millis(1708642103);
  specifics.set_name("my_name");
  specifics.add_data();
  specifics.mutable_data(0)->set_url("https://www.foo.com");
  specifics.add_data();
  specifics.mutable_data(1)->set_url("https://www.bar.com");

  base::Value::Dict value =
      ProductComparisonSpecificsToValue(specifics).TakeDict();
  EXPECT_FALSE(value.empty());
  EXPECT_TRUE(value.FindString("uuid"));
  EXPECT_STREQ("my_uuid", value.FindString("uuid")->c_str());
  EXPECT_TRUE(value.FindString("creation_time_unix_epoch_millis"));
  EXPECT_STREQ("1708532099",
               value.FindString("creation_time_unix_epoch_millis")->c_str());
  EXPECT_TRUE(value.FindString("update_time_unix_epoch_millis"));
  EXPECT_STREQ("1708642103",
               value.FindString("update_time_unix_epoch_millis")->c_str());
  EXPECT_TRUE(value.FindString("name"));
  EXPECT_STREQ("my_name", value.FindString("name")->c_str());
  const base::Value::List* data_list = value.FindList("data");
  EXPECT_TRUE(data_list);
  EXPECT_EQ(2u, data_list->size());
  EXPECT_TRUE((*data_list)[0].GetDict().FindString("url"));
  EXPECT_STREQ("https://www.foo.com",
               (*data_list)[0].GetDict().FindString("url")->c_str());
  EXPECT_TRUE((*data_list)[1].GetDict().FindString("url"));
  EXPECT_STREQ("https://www.bar.com",
               (*data_list)[1].GetDict().FindString("url")->c_str());
}

}  // namespace
}  // namespace syncer
