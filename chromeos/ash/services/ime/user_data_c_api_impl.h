// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_USER_DATA_C_API_IMPL_H_
#define CHROMEOS_ASH_SERVICES_IME_USER_DATA_C_API_IMPL_H_

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/user_data_service.pb.h"
#include "chromeos/ash/services/ime/user_data_c_api_interface.h"

namespace ash::ime {

class UserDataCApiImpl : public UserDataCApiInterface {
 public:
  UserDataCApiImpl(
      ImeCrosPlatform* platform,
      ImeSharedLibraryWrapper::EntryPoints shared_library_entry_points);

  ~UserDataCApiImpl() override;

  // Sends a UserDataRequest to the decoder.
  chromeos_input::UserDataResponse ProcessUserDataRequest(
      const chromeos_input::UserDataRequest& request) override;

 private:
  ImeSharedLibraryWrapper::EntryPoints shared_library_entry_points_;
};

}  // namespace ash::ime

#endif  // CHROMEOS_ASH_SERVICES_IME_USER_DATA_C_API_IMPL_H_
