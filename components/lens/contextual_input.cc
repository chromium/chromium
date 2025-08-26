// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/contextual_input.h"

namespace lens {

ContextualInput::ContextualInput() : content_type_(lens::MimeType::kUnknown) {}
ContextualInput::ContextualInput(std::vector<uint8_t> bytes,
                                 lens::MimeType content_type)
    : bytes_(bytes), content_type_(content_type) {}
ContextualInput::ContextualInput(const ContextualInput& other) = default;
ContextualInput::~ContextualInput() = default;

ContextualInputData::ContextualInputData() = default;
ContextualInputData::~ContextualInputData() = default;
ContextualInputData::ContextualInputData(const ContextualInputData&) = default;

}  // namespace lens
