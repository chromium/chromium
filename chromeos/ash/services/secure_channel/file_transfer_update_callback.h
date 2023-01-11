// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash::secure_channel {

// Syntactic sugar to make it easier to declare callbacks to the
// RegisterPayloadFile APIs.
using FileTransferUpdateCallback =
    base::RepeatingCallback<void(mojom::FileTransferUpdatePtr)>;

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FILE_TRANSFER_UPDATE_CALLBACK_H_
