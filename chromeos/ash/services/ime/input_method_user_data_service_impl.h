// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_INPUT_METHOD_USER_DATA_SERVICE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_IME_INPUT_METHOD_USER_DATA_SERVICE_IMPL_H_

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::ime {

class InputMethodUserDataServiceImpl
    : public mojom::InputMethodUserDataService {
 public:
  InputMethodUserDataServiceImpl(
      ImeCrosPlatform* platform,
      ImeSharedLibraryWrapper::EntryPoints shared_library_entry_points);

  ~InputMethodUserDataServiceImpl() override;

  void AddReceiver(
      mojo::PendingReceiver<mojom::InputMethodUserDataService> receiver);

 private:
  mojo::ReceiverSet<mojom::InputMethodUserDataService> receiver_set_;

  ImeSharedLibraryWrapper::EntryPoints shared_library_entry_points_;
};

}  // namespace ash::ime
#endif  // CHROMEOS_ASH_SERVICES_IME_INPUT_METHOD_USER_DATA_SERVICE_IMPL_H_
