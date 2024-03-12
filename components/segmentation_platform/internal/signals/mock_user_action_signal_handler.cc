// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/mock_user_action_signal_handler.h"

namespace segmentation_platform {

MockUserActionSignalHandler::MockUserActionSignalHandler()
    : UserActionSignalHandler("", nullptr, nullptr) {}

MockUserActionSignalHandler::~MockUserActionSignalHandler() = default;

}  // namespace segmentation_platform
