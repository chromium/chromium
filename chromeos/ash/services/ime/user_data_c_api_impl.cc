// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/user_data_c_api_impl.h"

#include "chromeos/ash/services/ime/ime_shared_library_wrapper.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/proto/user_data_service.pb.h"

namespace ash::ime {

UserDataCApiImpl::~UserDataCApiImpl() = default;

UserDataCApiImpl::UserDataCApiImpl(
    ImeCrosPlatform* platform,
    ImeSharedLibraryWrapper::EntryPoints shared_library_entry_points)
    : shared_library_entry_points_(shared_library_entry_points) {
  shared_library_entry_points_.init_user_data_service(platform);
}

chromeos_input::UserDataResponse UserDataCApiImpl::ProcessUserDataRequest(
    const chromeos_input::UserDataRequest& request) {
  // bytes.data is directly modified, so bytes.size() would be wrong if you
  // simply reserve the size. This construction would create default values of
  // uint8_t up to the size required, so that the vector knows the "size" of the
  // data that will be inserted into it.
  std::vector<uint8_t> bytes(request.ByteSizeLong());
  request.SerializeToArray(bytes.data(),
                           static_cast<int>(request.ByteSizeLong()));
  C_SerializedProto c_request{/* buffer= */ bytes.data(),
                              /* size= */ bytes.size()};

  // This response needs to be deleted manually to avoid a memory leak.
  // The buffer has to be made persistent in order to be read by chromium.
  C_SerializedProto c_response =
      shared_library_entry_points_.process_user_data_request(c_request);
  chromeos_input::UserDataResponse response;
  response.ParseFromArray(c_response.buffer, c_response.size);
  shared_library_entry_points_.delete_serialized_proto(c_response);

  return response;
}

}  // namespace ash::ime
