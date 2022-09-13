// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/input_context.h"

#include "components/segmentation_platform/public/trigger_context.h"

namespace segmentation_platform {

InputContext::InputContext() = default;

InputContext::InputContext(const TriggerContext& trigger_context)
    : metadata_args(trigger_context.GetSelectionInputArgs()) {}

InputContext::~InputContext() = default;

}  // namespace segmentation_platform
