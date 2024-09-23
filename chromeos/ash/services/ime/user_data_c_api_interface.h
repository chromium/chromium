// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_USER_DATA_C_API_INTERFACE_H_
#define CHROMEOS_ASH_SERVICES_IME_USER_DATA_C_API_INTERFACE_H_

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/user_data_service.pb.h"

namespace ash::ime {

class UserDataCApiInterface {
 public:
  virtual ~UserDataCApiInterface() = default;

  // Sends a UserDataRequest to the decoder.
  virtual chromeos_input::UserDataResponse ProcessUserDataRequest(
      const chromeos_input::UserDataRequest& request) = 0;
};

}  // namespace ash::ime

#endif  // CHROMEOS_ASH_SERVICES_IME_USER_DATA_C_API_INTERFACE_H_
