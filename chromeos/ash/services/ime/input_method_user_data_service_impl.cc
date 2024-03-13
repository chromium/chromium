// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/input_method_user_data_service_impl.h"
#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
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

void InputMethodUserDataServiceImpl::AddReceiver(
    mojo::PendingReceiver<mojom::InputMethodUserDataService> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

}  // namespace ime
}  // namespace ash
