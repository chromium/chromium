// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/input_method_user_data_service_impl.h"

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/fetch_japanese_legacy_config.pb.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/japanese_dictionary.pb.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/user_data_service.pb.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/user_data_japanese_legacy_config.mojom.h"
#include "chromeos/ash/services/ime/user_data/japanese_dictionary.h"
#include "chromeos/ash/services/ime/user_data/japanese_legacy_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {
namespace ime {

InputMethodUserDataServiceImpl::~InputMethodUserDataServiceImpl() = default;

InputMethodUserDataServiceImpl::InputMethodUserDataServiceImpl(
    ImeCrosPlatform* platform,
    ImeSharedLibraryWrapper::EntryPoints shared_library_entry_points)
    : shared_library_entry_points_(shared_library_entry_points) {
  if (shared_library_entry_points_.init_user_data_service) {
    shared_library_entry_points_.init_user_data_service(platform);
  } else {
    LOG(ERROR) << "sharedlib init_user_data_service not intialized";
  }
}

void InputMethodUserDataServiceImpl::FetchJapaneseLegacyConfig(
    FetchJapaneseLegacyConfigCallback callback) {
  chromeos_input::UserDataRequest request;
  chromeos_input::FetchJapaneseLegacyConfigRequest fetch_request;
  *request.mutable_fetch_japanese_legacy_config() = fetch_request;
  chromeos_input::UserDataResponse user_data_response =
      ProcessUserDataRequest(request);

  if (user_data_response.status().success() &&
      user_data_response.has_fetch_japanese_legacy_config()) {
    mojom::JapaneseLegacyConfigPtr response_data =
        MakeMojomJapaneseLegacyConfig(
            user_data_response.fetch_japanese_legacy_config());
    mojom::JapaneseLegacyConfigResponsePtr response =
        mojom::JapaneseLegacyConfigResponse::NewResponse(
            std::move(response_data));
    std::move(callback).Run(std::move(response));
  } else {
    mojom::JapaneseLegacyConfigResponsePtr response =
        mojom::JapaneseLegacyConfigResponse::NewErrorReason(
            user_data_response.status().reason());
    std::move(callback).Run(std::move(response));
  }
}

void InputMethodUserDataServiceImpl::FetchJapaneseDictionary(
    FetchJapaneseDictionaryCallback callback) {
  chromeos_input::UserDataRequest request;
  chromeos_input::FetchJapaneseDictionaryRequest fetch_request;
  *request.mutable_fetch_japanese_dictionary() = fetch_request;

  chromeos_input::UserDataResponse user_data_response =
      ProcessUserDataRequest(request);

  mojom::JapaneseDictionaryResponsePtr response =
      mojom::JapaneseDictionaryResponse::NewDictionaries({});
  std::vector<mojom::JapaneseDictionaryPtr> dictionaries;
  for (const auto& dict :
       user_data_response.fetch_japanese_dictionary().dictionaries()) {
    mojom::JapaneseDictionaryPtr data = MakeMojomJapaneseDictionary(dict);
    response->get_dictionaries().push_back(std::move(data));
  }

  std::move(callback).Run(std::move(response));
}

void InputMethodUserDataServiceImpl::AddReceiver(
    mojo::PendingReceiver<mojom::InputMethodUserDataService> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

chromeos_input::UserDataResponse
InputMethodUserDataServiceImpl::ProcessUserDataRequest(
    chromeos_input::UserDataRequest request_proto) {
  std::vector<uint8_t> bytes;
  bytes.resize(request_proto.ByteSizeLong());
  request_proto.SerializeToArray(
      bytes.data(), static_cast<int>(request_proto.ByteSizeLong()));
  C_SerializedProto request{/* buffer= */ bytes.data(),
                            /* size= */ bytes.size()};

  // This response needs to be deleted manually to avoid a memory leak.
  // The buffer has to be made persistent in order to be read by chromium.
  C_SerializedProto response =
      shared_library_entry_points_.process_user_data_request(request);
  chromeos_input::UserDataResponse response_proto;
  response_proto.ParseFromArray(response.buffer, response.size);
  shared_library_entry_points_.delete_serialized_proto(response);

  return response_proto;
}

}  // namespace ime
}  // namespace ash
