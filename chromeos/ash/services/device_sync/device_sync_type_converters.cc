// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/device_sync_type_converters.h"

namespace mojo {

// static
ash::device_sync::mojom::NetworkRequestResult
TypeConverter<ash::device_sync::mojom::NetworkRequestResult,
              ash::device_sync::NetworkRequestError>::
    Convert(ash::device_sync::NetworkRequestError type) {
  switch (type) {
    case ash::device_sync::NetworkRequestError::kOffline:
      return ash::device_sync::mojom::NetworkRequestResult::kOffline;
    case ash::device_sync::NetworkRequestError::kEndpointNotFound:
      return ash::device_sync::mojom::NetworkRequestResult::kEndpointNotFound;
    case ash::device_sync::NetworkRequestError::kAuthenticationError:
      return ash::device_sync::mojom::NetworkRequestResult::
          kAuthenticationError;
    case ash::device_sync::NetworkRequestError::kBadRequest:
      return ash::device_sync::mojom::NetworkRequestResult::kBadRequest;
    case ash::device_sync::NetworkRequestError::kResponseMalformed:
      return ash::device_sync::mojom::NetworkRequestResult::kResponseMalformed;
    case ash::device_sync::NetworkRequestError::kInternalServerError:
      return ash::device_sync::mojom::NetworkRequestResult::
          kInternalServerError;
    case ash::device_sync::NetworkRequestError::kUnknown:
      return ash::device_sync::mojom::NetworkRequestResult::kUnknown;
  }
}

}  // namespace mojo
