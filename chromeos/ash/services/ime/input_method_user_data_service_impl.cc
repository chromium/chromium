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
namespace {

namespace mojom = ::ash::ime::mojom;

}

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

void InputMethodUserDataServiceImpl::AddJapaneseDictionaryEntry(
    uint64_t dict_id,
    mojom::JapaneseDictionaryEntryPtr entry,
    AddJapaneseDictionaryEntryCallback callback) {
  chromeos_input::UserDataRequest user_data_request;

  chromeos_input::AddJapaneseDictionaryEntryRequest& request =
      *user_data_request.mutable_add_japanese_dictionary_entry();
  request.set_dictionary_id(dict_id);
  *request.mutable_entry() = MakeProtoJpDictEntry(*entry);

  chromeos_input::UserDataResponse user_data_response =
      c_api_->ProcessUserDataRequest(user_data_request);

  mojom::StatusPtr response = mojom::Status::New();
  response->success = user_data_response.status().success();
  if (user_data_response.status().has_reason()) {
    response->reason = user_data_response.status().reason();
  }
  std::move(callback).Run(std::move(response));
}

void InputMethodUserDataServiceImpl::EditJapaneseDictionaryEntry(
    uint64_t dict_id,
    uint32_t entry_index,
    ash::ime::mojom::JapaneseDictionaryEntryPtr entry,
    EditJapaneseDictionaryEntryCallback callback) {
  chromeos_input::UserDataRequest user_data_request;

  chromeos_input::EditJapaneseDictionaryEntryRequest& request =
      *user_data_request.mutable_edit_japanese_dictionary_entry();
  request.set_dictionary_id(dict_id);
  request.set_entry_index(entry_index);
  *request.mutable_entry() = MakeProtoJpDictEntry(*entry);

  chromeos_input::UserDataResponse user_data_response =
      c_api_->ProcessUserDataRequest(user_data_request);

  mojom::StatusPtr response = mojom::Status::New();
  response->success = user_data_response.status().success();
  if (user_data_response.status().has_reason()) {
    response->reason = user_data_response.status().reason();
  }
  std::move(callback).Run(std::move(response));
}

void InputMethodUserDataServiceImpl::DeleteJapaneseDictionaryEntry(
    uint64_t dict_id,
    uint32_t entry_index,
    DeleteJapaneseDictionaryEntryCallback callback) {
  chromeos_input::UserDataRequest user_data_request;

  chromeos_input::DeleteJapaneseDictionaryEntryRequest& request =
      *user_data_request.mutable_delete_japanese_dictionary_entry();
  request.set_dictionary_id(dict_id);
  request.set_entry_index(entry_index);

  chromeos_input::UserDataResponse user_data_response =
      c_api_->ProcessUserDataRequest(user_data_request);

  mojom::StatusPtr response = mojom::Status::New();
  response->success = user_data_response.status().success();
  if (user_data_response.status().has_reason()) {
    response->reason = user_data_response.status().reason();
  }
  std::move(callback).Run(std::move(response));
}

void InputMethodUserDataServiceImpl::CreateJapaneseDictionary(
    const std::string& dictionary_name,
    CreateJapaneseDictionaryCallback callback) {
  chromeos_input::UserDataRequest user_data_request;
  user_data_request.mutable_create_japanese_dictionary()->set_name(
      dictionary_name);

  chromeos_input::UserDataResponse user_data_response =
      c_api_->ProcessUserDataRequest(user_data_request);

  mojom::StatusPtr response = mojom::Status::New();
  response->success = user_data_response.status().success();
  if (user_data_response.status().has_reason()) {
    response->reason = user_data_response.status().reason();
  }
  std::move(callback).Run(std::move(response));
}

void InputMethodUserDataServiceImpl::RenameJapaneseDictionary(
    uint64_t dict_id,
    const std::string& dictionary_name,
    RenameJapaneseDictionaryCallback callback) {
  chromeos_input::UserDataRequest user_data_request;
  user_data_request.mutable_rename_japanese_dictionary()->set_dictionary_id(
      dict_id);
  user_data_request.mutable_rename_japanese_dictionary()->set_name(
      dictionary_name);

  chromeos_input::UserDataResponse user_data_response =
      c_api_->ProcessUserDataRequest(user_data_request);

  mojom::StatusPtr response = mojom::Status::New();
  response->success = user_data_response.status().success();
  if (user_data_response.status().has_reason()) {
    response->reason = user_data_response.status().reason();
  }
  std::move(callback).Run(std::move(response));
}

void InputMethodUserDataServiceImpl::DeleteJapaneseDictionary(
    uint64_t dict_id,
    DeleteJapaneseDictionaryCallback callback) {
  chromeos_input::UserDataRequest user_data_request;
  user_data_request.mutable_delete_japanese_dictionary()->set_dictionary_id(
      dict_id);

  chromeos_input::UserDataResponse user_data_response =
      c_api_->ProcessUserDataRequest(user_data_request);

  mojom::StatusPtr response = mojom::Status::New();
  response->success = user_data_response.status().success();
  if (user_data_response.status().has_reason()) {
    response->reason = user_data_response.status().reason();
  }
  std::move(callback).Run(std::move(response));
}

void InputMethodUserDataServiceImpl::AddReceiver(
    mojo::PendingReceiver<mojom::InputMethodUserDataService> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

}  // namespace ime
}  // namespace ash
