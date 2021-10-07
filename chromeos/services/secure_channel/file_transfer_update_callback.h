// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_

#include "base/callback.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace chromeos {

namespace secure_channel {

// Syntactic sugar to make it easier to declare callbacks to the
// RegisterPayloadFile APIs.
using FileTransferUpdateCallback =
    base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>;

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_
