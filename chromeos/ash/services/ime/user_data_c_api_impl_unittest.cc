// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/user_data_c_api_impl.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::ime {
namespace {

TEST(UserDataCApiTest,
     ProcessUserDataRequestSendsRequestToEntryPointAndHandlesResponse) {
  ImeSharedLibraryWrapper::EntryPoints entry_points;
  entry_points.init_user_data_service = [](ImeCrosPlatform* platform) {};
  // If input contains a fetch_legacy_config_request, it will return a
  // serialized response proto with FetchJapaneseLegacyConfigResponse.
  entry_points.process_user_data_request =
      [](C_SerializedProto c_request) -> C_SerializedProto {
    chromeos_input::UserDataRequest request;
    request.ParseFromArray(c_request.buffer, c_request.size);

    if (!request.has_fetch_japanese_legacy_config()) {
      return C_SerializedProto{/* buffer= */ 0, /* size= */ 0};
    }

    chromeos_input::UserDataResponse response;
    chromeos_input::Status& status = *response.mutable_status();
    status.set_success(true);
    chromeos_input::FetchJapaneseLegacyConfigResponse& japanese_response =
        *response.mutable_fetch_japanese_legacy_config();
    japanese_response.set_preedit_method(chromeos_input::PREEDIT_KANA);

    const size_t resp_byte_size = response.ByteSizeLong();
    auto* const resp_bytes = new uint8_t[resp_byte_size]();
    response.SerializeToArray(resp_bytes, static_cast<int>(resp_byte_size));
    return C_SerializedProto{/* buffer= */ resp_bytes,
                             /* size= */ resp_byte_size};
  };
  // Deletes the C_SerilizedProto created above;
  entry_points.delete_serialized_proto = [](C_SerializedProto proto) {
    delete[] proto.buffer;
  };
  UserDataCApiImpl c_api(nullptr, entry_points);
  chromeos_input::UserDataRequest request;
  *request.mutable_fetch_japanese_legacy_config() =
      chromeos_input::FetchJapaneseLegacyConfigRequest();

  chromeos_input::UserDataResponse response =
      c_api.ProcessUserDataRequest(request);

  EXPECT_TRUE(response.status().success());
  EXPECT_EQ(response.fetch_japanese_legacy_config().preedit_method(),
            chromeos_input::PREEDIT_KANA);
}

}  // namespace
}  // namespace ash::ime
