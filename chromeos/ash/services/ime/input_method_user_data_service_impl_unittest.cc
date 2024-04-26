// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/input_method_user_data_service_impl.h"

#include "base/test/test_future.h"
#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/fetch_japanese_legacy_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::ime {
namespace {

using base::test::TestFuture;
using chromeos_input::JapaneseDictionary;

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
  ImeSharedLibraryWrapper::EntryPoints entry_points;

  entry_points.init_user_data_service = [](ImeCrosPlatform* platform) {};
  entry_points.process_user_data_request =
      [](C_SerializedProto request) -> C_SerializedProto {
    chromeos_input::UserDataResponse response;
    chromeos_input::Status status;
    status.set_success(true);
    *response.mutable_status() = status;
    chromeos_input::FetchJapaneseLegacyConfigResponse japanese_response;
    japanese_response.set_preedit_method(chromeos_input::PREEDIT_KANA);
    *response.mutable_fetch_japanese_legacy_config() = japanese_response;

    const size_t resp_byte_size = response.ByteSizeLong();
    auto* const resp_bytes = new uint8_t[resp_byte_size]();
    response.SerializeToArray(resp_bytes, static_cast<int>(resp_byte_size));
    return C_SerializedProto{/* buffer= */ resp_bytes,
                             /* size= */ resp_byte_size};
  };
  entry_points.delete_serialized_proto = [](C_SerializedProto proto) {
    delete[] proto.buffer;
  };

  TestFuture<mojom::JapaneseLegacyConfigResponsePtr> config_future;
  InputMethodUserDataServiceImpl service(nullptr, entry_points);

  service.FetchJapaneseLegacyConfig(config_future.GetCallback());

  const mojom::JapaneseLegacyConfigResponsePtr& response = config_future.Get();

  mojom::JapaneseLegacyConfigPtr expected_config =
      mojom::JapaneseLegacyConfig::New();
  expected_config->preedit_method =
      mojom::JapaneseLegacyConfig::PreeditMethod::kKana;
  mojom::JapaneseLegacyConfigResponsePtr expected =
      mojom::JapaneseLegacyConfigResponse::NewResponse(
          std::move(expected_config));

  EXPECT_TRUE(response.Equals(expected));
}

TEST(InputMethodUserDataServiceTest, FetchJapaneseDictionary) {
  ImeSharedLibraryWrapper::EntryPoints entry_points;

  entry_points.init_user_data_service = [](ImeCrosPlatform* platform) {};
  entry_points.process_user_data_request =
      [](C_SerializedProto request) -> C_SerializedProto {
    chromeos_input::UserDataResponse response;
    chromeos_input::Status status;
    status.set_success(true);
    *response.mutable_status() = status;
    chromeos_input::FetchJapaneseDictionaryResponse fetch_response;

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

    *response.mutable_fetch_japanese_dictionary() = fetch_response;

    const size_t resp_byte_size = response.ByteSizeLong();
    auto* const resp_bytes = new uint8_t[resp_byte_size]();
    response.SerializeToArray(resp_bytes, static_cast<int>(resp_byte_size));
    return C_SerializedProto{/* buffer= */ resp_bytes,
                             /* size= */ resp_byte_size};
  };
  entry_points.delete_serialized_proto = [](C_SerializedProto proto) {
    delete[] proto.buffer;
  };
  TestFuture<mojom::JapaneseDictionaryResponsePtr> config_future;
  InputMethodUserDataServiceImpl service(nullptr, entry_points);

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
  const mojom::JapaneseDictionaryResponsePtr& response = config_future.Get();
  EXPECT_EQ(response, expected);
}

}  // namespace
}  // namespace ash::ime
