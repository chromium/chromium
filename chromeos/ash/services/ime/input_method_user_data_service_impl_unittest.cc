// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/input_method_user_data_service_impl.h"

#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/fetch_japanese_legacy_config.pb.h"
#include "chromeos/ash/services/ime/user_data_c_api_impl.h"
#include "chromeos/ash/services/ime/user_data_c_api_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::ime {
namespace {

class MockCApi : public UserDataCApiInterface {
 public:
  MOCK_METHOD(chromeos_input::UserDataResponse,
              ProcessUserDataRequest,
              (const chromeos_input::UserDataRequest& request),
              (override));
};

using ::base::test::EqualsProto;
using base::test::TestFuture;
using chromeos_input::JapaneseDictionary;
using ::testing::_;
using ::testing::Return;

void SetEntry(JapaneseDictionary::Entry& entry,
              const std::string& key,
              const std::string& value,
              const std::string& comment,
              JapaneseDictionary::PosType pos) {
  entry.set_key(key);
  entry.set_value(value);
  entry.set_comment(comment);
  entry.set_pos(pos);
}

TEST(InputMethodUserDataServiceTest, FetchJapaneseLegacyConfig) {
  chromeos_input::UserDataRequest request_pb;
  *request_pb.mutable_fetch_japanese_legacy_config() =
      chromeos_input::FetchJapaneseLegacyConfigRequest();
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(true);
  response_pb.mutable_fetch_japanese_legacy_config()->set_preedit_method(
      chromeos_input::PREEDIT_KANA);
  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::JapaneseLegacyConfigResponsePtr> config_future;
  service.FetchJapaneseLegacyConfig(config_future.GetCallback());

  mojom::JapaneseLegacyConfigPtr expected_config =
      mojom::JapaneseLegacyConfig::New();
  expected_config->preedit_method =
      mojom::JapaneseLegacyConfig::PreeditMethod::kKana;
  mojom::JapaneseLegacyConfigResponsePtr expected =
      mojom::JapaneseLegacyConfigResponse::NewResponse(
          std::move(expected_config));
  EXPECT_TRUE(config_future.Get().Equals(expected));
}

TEST(InputMethodUserDataServiceTest, FetchJapaneseDictionary) {
  chromeos_input::UserDataRequest request_pb;
  *request_pb.mutable_fetch_japanese_dictionary() =
      chromeos_input::FetchJapaneseDictionaryRequest();
  chromeos_input::UserDataResponse response_pb;
  chromeos_input::Status status;
  status.set_success(true);
  *response_pb.mutable_status() = status;
  chromeos_input::FetchJapaneseDictionaryResponse& fetch_response =
      *response_pb.mutable_fetch_japanese_dictionary();
  // Dictionary 1
  chromeos_input::JapaneseDictionary& personal_dictionary =
      *fetch_response.add_dictionaries();
  personal_dictionary.set_id(1);
  personal_dictionary.set_name("Personal Dictionary");
  SetEntry(/*entry=*/*personal_dictionary.add_entries(), /*key=*/"めーる",
           /*value=*/"non.example@gmail.com", /*comment=*/"My email",
           /*pos=*/JapaneseDictionary::ABBREVIATION);
  SetEntry(/*entry=*/*personal_dictionary.add_entries(), /*key=*/"そうし",
           /*value=*/"草詩", /*comment=*/"My name",
           /*pos=*/JapaneseDictionary::PERSONAL_NAME);
  // Dictionary 2
  chromeos_input::JapaneseDictionary& writing_dictionary =
      *fetch_response.add_dictionaries();
  writing_dictionary.set_id(2);
  writing_dictionary.set_name("Writing Dictionary");
  SetEntry(
      /*entry=*/*writing_dictionary.add_entries(), /*key=*/"その",
      /*value=*/"その違いを日本語で解説してください（表形式）。",
      /*comment=*/"Please explain the difference in Japanese (table format).",
      /*pos=*/JapaneseDictionary::ABBREVIATION);
  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::JapaneseDictionaryResponsePtr> config_future;
  service.FetchJapaneseDictionary(config_future.GetCallback());

  mojom::JapaneseDictionaryResponsePtr expected =
      mojom::JapaneseDictionaryResponse::NewDictionaries({});
  // Dictionary 1
  mojom::JapaneseDictionaryPtr dict_1 = mojom::JapaneseDictionary::New();
  dict_1->id = 1;
  dict_1->name = "Personal Dictionary";
  dict_1->entries.push_back(mojom::JapaneseDictionaryEntry::New(
      /*key=*/"めーる",
      /*value=*/"non.example@gmail.com",
      /*pos=*/mojom::JpPosType::kAbbreviation,
      /*comment=*/"My email"));
  dict_1->entries.push_back(mojom::JapaneseDictionaryEntry::New(
      /*key=*/"そうし",
      /*value=*/"草詩",
      /*pos=*/mojom::JpPosType::kPersonalName,
      /*comment=*/"My name"));
  expected->get_dictionaries().push_back(std::move(dict_1));
  // Dictionary 2
  mojom::JapaneseDictionaryPtr dict_2 = mojom::JapaneseDictionary::New();
  dict_2->id = 2;
  dict_2->name = "Writing Dictionary";
  dict_2->entries.push_back(mojom::JapaneseDictionaryEntry::New(
      /*key=*/"その",
      /*value=*/"その違いを日本語で解説してください（表形式）。",
      /*pos=*/mojom::JpPosType::kAbbreviation,
      /*comment=*/"Please explain the difference in Japanese (table format)."));
  expected->get_dictionaries().push_back(std::move(dict_2));
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, AddJapaneseDictionaryEntryOnSuccess) {
  chromeos_input::UserDataRequest request_pb;
  chromeos_input::AddJapaneseDictionaryEntryRequest& add_request =
      *request_pb.mutable_add_japanese_dictionary_entry();
  add_request.set_dictionary_id(999);
  add_request.mutable_entry()->set_key("key");
  add_request.mutable_entry()->set_value("value");
  add_request.mutable_entry()->set_comment("comment");
  add_request.mutable_entry()->set_pos(JapaneseDictionary::FIRST_NAME);
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(true);

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  mojom::JapaneseDictionaryEntryPtr entry = mojom::JapaneseDictionaryEntry::New(
      /*key=*/"key", /*value=*/"value", /*pos=*/mojom::JpPosType::kFirstName,
      /*comment=*/"comment");
  service.AddJapaneseDictionaryEntry(/*dict_id=*/999, std::move(entry),
                                     config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = true;
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, AddJapaneseDictionaryEntryOnError) {
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(false);
  response_pb.mutable_status()->set_reason("Unknown Error");

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest)
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  mojom::JapaneseDictionaryEntryPtr entry = mojom::JapaneseDictionaryEntry::New(
      /*key=*/"key", /*value=*/"value", /*pos=*/mojom::JpPosType::kFirstName,
      /*comment=*/"comment");
  service.AddJapaneseDictionaryEntry(/*dict_id=*/999, std::move(entry),
                                     config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = false;
  expected->reason = "Unknown Error";
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, EditJapaneseDictionaryEntryOnSuccess) {
  chromeos_input::UserDataRequest request_pb;
  chromeos_input::EditJapaneseDictionaryEntryRequest& edit_request =
      *request_pb.mutable_edit_japanese_dictionary_entry();
  edit_request.set_dictionary_id(999);
  edit_request.set_entry_index(1);
  edit_request.mutable_entry()->set_key("key");
  edit_request.mutable_entry()->set_value("value");
  edit_request.mutable_entry()->set_comment("comment");
  edit_request.mutable_entry()->set_pos(JapaneseDictionary::FIRST_NAME);
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(true);

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  mojom::JapaneseDictionaryEntryPtr entry = mojom::JapaneseDictionaryEntry::New(
      /*key=*/"key", /*value=*/"value", /*pos=*/mojom::JpPosType::kFirstName,
      /*comment=*/"comment");
  service.EditJapaneseDictionaryEntry(/*dict_id=*/999, /*entry_index=*/1,
                                      std::move(entry),
                                      config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = true;
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, EditJapaneseDictionaryEntryOnError) {
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(false);
  response_pb.mutable_status()->set_reason("Unknown Error");

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest)
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  mojom::JapaneseDictionaryEntryPtr entry = mojom::JapaneseDictionaryEntry::New(
      /*key=*/"key", /*value=*/"value", /*pos=*/mojom::JpPosType::kFirstName,
      /*comment=*/"comment");
  service.EditJapaneseDictionaryEntry(/*dict_id=*/999, /*entry_index=*/1,
                                      std::move(entry),
                                      config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = false;
  expected->reason = "Unknown Error";
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, DeleteJapaneseDictionaryEntryOnSuccess) {
  chromeos_input::UserDataRequest request_pb;
  chromeos_input::DeleteJapaneseDictionaryEntryRequest& delete_request =
      *request_pb.mutable_delete_japanese_dictionary_entry();
  delete_request.set_dictionary_id(999);
  delete_request.set_entry_index(1);
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(true);

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.DeleteJapaneseDictionaryEntry(/*dict_id=*/999, /*entry_index=*/1,
                                        config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = true;
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, DeleteJapaneseDictionaryEntryOnError) {
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(false);
  response_pb.mutable_status()->set_reason("Unknown Error");

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest)
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.DeleteJapaneseDictionaryEntry(/*dict_id=*/999, /*entry_index=*/1,
                                        config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = false;
  expected->reason = "Unknown Error";
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, CreateJapaneseDictionaryOnSuccess) {
  chromeos_input::UserDataRequest request_pb;
  request_pb.mutable_create_japanese_dictionary()->set_name("name");
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(true);

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.CreateJapaneseDictionary(/*dictionary_name=*/"name",
                                   config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = true;
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, CreateJapaneseDictionaryOnError) {
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(false);
  response_pb.mutable_status()->set_reason("Unknown Error");

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest)
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.CreateJapaneseDictionary(/*dictionary_name=*/"name",
                                   config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = false;
  expected->reason = "Unknown Error";
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, RenameJapaneseDictionaryOnSuccess) {
  chromeos_input::UserDataRequest request_pb;
  request_pb.mutable_rename_japanese_dictionary()->set_dictionary_id(999);
  request_pb.mutable_rename_japanese_dictionary()->set_name("name");
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(true);

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.RenameJapaneseDictionary(/*dict_id=*/999,
                                   /*dictionary_name=*/"name",
                                   config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = true;
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, RenameJapaneseDictionaryOnError) {
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(false);
  response_pb.mutable_status()->set_reason("Unknown Error");

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest)
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.RenameJapaneseDictionary(
      /*dict_id=*/999,
      /*dictionary_name=*/"name", config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = false;
  expected->reason = "Unknown Error";
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, DeleteJapaneseDictionaryOnSuccess) {
  chromeos_input::UserDataRequest request_pb;
  request_pb.mutable_delete_japanese_dictionary()->set_dictionary_id(999);
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(true);

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest(EqualsProto(request_pb)))
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.DeleteJapaneseDictionary(/*dict_id=*/999,
                                   config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = true;
  EXPECT_EQ(config_future.Get(), expected);
}

TEST(InputMethodUserDataServiceTest, DeleteJapaneseDictionaryOnError) {
  chromeos_input::UserDataResponse response_pb;
  response_pb.mutable_status()->set_success(false);
  response_pb.mutable_status()->set_reason("Unknown Error");

  std::unique_ptr<MockCApi> c_api = std::make_unique<MockCApi>();
  EXPECT_CALL(*c_api, ProcessUserDataRequest)
      .Times(1)
      .WillOnce(Return(response_pb));

  InputMethodUserDataServiceImpl service(std::move(c_api));
  TestFuture<mojom::StatusPtr> config_future;
  service.DeleteJapaneseDictionary(
      /*dict_id=*/999, config_future.GetCallback());

  mojom::StatusPtr expected = mojom::Status::New();
  expected->success = false;
  expected->reason = "Unknown Error";
  EXPECT_EQ(config_future.Get(), expected);
}

}  // namespace
}  // namespace ash::ime
