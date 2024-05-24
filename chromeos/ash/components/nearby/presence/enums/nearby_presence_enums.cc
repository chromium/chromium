// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/enums/nearby_presence_enums.h"

namespace ash::nearby::presence::enums {

StatusCode ConvertToPresenceStatus(
    mojo_base::mojom::AbslStatusCode status_code) {
  switch (status_code) {
    case mojo_base::mojom::AbslStatusCode::kOk:
      return StatusCode::kAbslOk;
    case mojo_base::mojom::AbslStatusCode::kCancelled:
      return StatusCode::kAbslCancelled;
    case mojo_base::mojom::AbslStatusCode::kUnknown:
      return StatusCode::kAbslUnknown;
    case mojo_base::mojom::AbslStatusCode::kInvalidArgument:
      return StatusCode::kAbslInvalidArgument;
    case mojo_base::mojom::AbslStatusCode::kDeadlineExceeded:
      return StatusCode::kAbslDeadlineExceeded;
    case mojo_base::mojom::AbslStatusCode::kNotFound:
      return StatusCode::kAbslNotFound;
    case mojo_base::mojom::AbslStatusCode::kAlreadyExists:
      return StatusCode::kAbslAlreadyExists;
    case mojo_base::mojom::AbslStatusCode::kPermissionDenied:
      return StatusCode::kAbslPermissionDenied;
    case mojo_base::mojom::AbslStatusCode::kResourceExhausted:
      return StatusCode::kAbslResourceExhausted;
    case mojo_base::mojom::AbslStatusCode::kFailedPrecondition:
      return StatusCode::kAbslFailedPrecondition;
    case mojo_base::mojom::AbslStatusCode::kAborted:
      return StatusCode::kAbslAborted;
    case mojo_base::mojom::AbslStatusCode::kOutOfRange:
      return StatusCode::kAbslOutOfRange;
    case mojo_base::mojom::AbslStatusCode::kUnimplemented:
      return StatusCode::kAbslUnimplemented;
    case mojo_base::mojom::AbslStatusCode::kInternal:
      return StatusCode::kAbslInternal;
    case mojo_base::mojom::AbslStatusCode::kUnavailable:
      return StatusCode::kAbslUnavailable;
    case mojo_base::mojom::AbslStatusCode::kDataLoss:
      return StatusCode::kAbslDataLoss;
    case mojo_base::mojom::AbslStatusCode::kUnauthenticated:
      return StatusCode::kAbslUnauthenticated;
  }
}

}  // namespace ash::nearby::presence::enums
