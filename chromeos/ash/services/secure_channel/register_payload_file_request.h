// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_REGISTER_PAYLOAD_FILE_REQUEST_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_REGISTER_PAYLOAD_FILE_REQUEST_H_

#include "chromeos/ash/services/secure_channel/file_transfer_update_callback.h"

namespace ash::secure_channel {

// Captures arguments of RegisterPayloadFile() calls to help verification in
// tests.
struct RegisterPayloadFileRequest {
  RegisterPayloadFileRequest(
      int64_t payload_id,
      FileTransferUpdateCallback file_transfer_update_callback);
  // This struct is move-only so that it can be placed into containers due to
  // the |file_transfer_update_callback| field.
  RegisterPayloadFileRequest(RegisterPayloadFileRequest&&);
  RegisterPayloadFileRequest& operator=(RegisterPayloadFileRequest&&);
  ~RegisterPayloadFileRequest();

  int64_t payload_id;
  FileTransferUpdateCallback file_transfer_update_callback;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_REGISTER_PAYLOAD_FILE_REQUEST_H_
