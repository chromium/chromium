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

using base::test::TestFuture;

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

}  // namespace ash::ime
