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
#include "chromeos/ash/services/ime/user_data_c_api_interface.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {
namespace ime {

InputMethodUserDataServiceImpl::~InputMethodUserDataServiceImpl() = default;

InputMethodUserDataServiceImpl::InputMethodUserDataServiceImpl(
    std::unique_ptr<UserDataCApiInterface> c_api)
    : c_api_(std::move(c_api)) {}

void InputMethodUserDataServiceImpl::FetchJapaneseLegacyConfig(
    FetchJapaneseLegacyConfigCallback callback) {
  chromeos_input::UserDataRequest request;
  chromeos_input::FetchJapaneseLegacyConfigRequest fetch_request;
  *request.mutable_fetch_japanese_legacy_config() = fetch_request;
  chromeos_input::UserDataResponse user_data_response =
      c_api_->ProcessUserDataRequest(request);

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
      c_api_->ProcessUserDataRequest(request);

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

}  // namespace ime
}  // namespace ash
