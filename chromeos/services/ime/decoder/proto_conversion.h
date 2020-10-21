// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_DECODER_PROTO_CONVERSION_H_
#define CHROMEOS_SERVICES_IME_DECODER_PROTO_CONVERSION_H_

#include <vector>

#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/ime/public/proto/messages.pb.h"

namespace chromeos {
namespace ime {

// Converts arguments of a Mojo call to InputChannel::OnFocus into a proto.
ime::PublicMessage OnFocusToProto(uint64_t seq_id);

// Converts arguments of a Mojo call to InputChannel::OnKeyEvent into a proto.
ime::PublicMessage OnKeyEventToProto(uint64_t seq_id,
                                     mojom::PhysicalKeyEventPtr event);

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_DECODER_PROTO_CONVERSION_H_
