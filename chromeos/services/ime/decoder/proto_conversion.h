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

// Converts arguments of a Mojo call to InputChannel::OnInputMethodChanged into
// a proto.
ime::PublicMessage OnInputMethodChangedToProto(uint64_t seq_id,
                                               const std::string& engine_id);

// Converts arguments of a Mojo call to InputChannel::OnFocus into a proto.
ime::PublicMessage OnFocusToProto(uint64_t seq_id,
                                  mojom::InputFieldInfoPtr input_field_info);

// Converts arguments of a Mojo call to InputChannel::OnBlur into a proto.
ime::PublicMessage OnBlurToProto(uint64_t seq_id);

// Converts arguments of a Mojo call to InputChannel::OnKeyEvent into a proto.
ime::PublicMessage OnKeyEventToProto(uint64_t seq_id,
                                     mojom::PhysicalKeyEventPtr event);

// Converts arguments of a Mojo call to InputChannel::OnSurroundingTextChanged
// into a proto.
ime::PublicMessage OnSurroundingTextChangedToProto(
    uint64_t seq_id,
    const std::string& text,
    uint32_t focus,
    mojom::SelectionRangePtr selection_range);

// Converts arguments of a Mojo call to InputChannel::OnCompositionCanceled into
// a proto.
ime::PublicMessage OnCompositionCanceledToProto(uint64_t seq_id);

// Converts a proto to InputChannel::AutocorrectSpan.
mojom::AutocorrectSpanPtr ProtoToAutocorrectSpan(
    const chromeos::ime::AutocorrectSpan& autocorrect_span);

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_DECODER_PROTO_CONVERSION_H_
