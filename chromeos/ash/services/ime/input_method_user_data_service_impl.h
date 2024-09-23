// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_INPUT_METHOD_USER_DATA_SERVICE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_IME_INPUT_METHOD_USER_DATA_SERVICE_IMPL_H_

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/fetch_japanese_legacy_config.pb.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/user_data_service.pb.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "chromeos/ash/services/ime/user_data_c_api_interface.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::ime {

class InputMethodUserDataServiceImpl
    : public mojom::InputMethodUserDataService {
 public:
  InputMethodUserDataServiceImpl(std::unique_ptr<UserDataCApiInterface> c_api);

  ~InputMethodUserDataServiceImpl() override;

  void FetchJapaneseLegacyConfig(
      FetchJapaneseLegacyConfigCallback callback) override;

  void FetchJapaneseDictionary(
      FetchJapaneseDictionaryCallback callback) override;

  void AddJapaneseDictionaryEntry(
      uint64_t dict_id,
      ash::ime::mojom::JapaneseDictionaryEntryPtr entry,
      AddJapaneseDictionaryEntryCallback callback) override;

  void EditJapaneseDictionaryEntry(
      uint64_t dict_id,
      uint32_t entry_index,
      ash::ime::mojom::JapaneseDictionaryEntryPtr entry,
      EditJapaneseDictionaryEntryCallback callback) override;

  void DeleteJapaneseDictionaryEntry(
      uint64_t dict_id,
      uint32_t entry_index,
      DeleteJapaneseDictionaryEntryCallback callback) override;

  void CreateJapaneseDictionary(
      const std::string& dictionary_name,
      CreateJapaneseDictionaryCallback callback) override;

  void RenameJapaneseDictionary(
      uint64_t dict_id,
      const std::string& dictionary_name,
      RenameJapaneseDictionaryCallback callback) override;

  void DeleteJapaneseDictionary(
      uint64_t dict_id,
      DeleteJapaneseDictionaryCallback callback) override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::InputMethodUserDataService> receiver);

 private:
  mojo::ReceiverSet<mojom::InputMethodUserDataService> receiver_set_;

  std::unique_ptr<UserDataCApiInterface> c_api_;
};

}  // namespace ash::ime
#endif  // CHROMEOS_ASH_SERVICES_IME_INPUT_METHOD_USER_DATA_SERVICE_IMPL_H_
