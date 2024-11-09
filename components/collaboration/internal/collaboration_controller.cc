// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_controller.h"

namespace collaboration {

CollaborationController::CollaborationController(
    std::unique_ptr<CollaborationControllerDelegate> delegate)
    : delegate_(std::move(delegate)) {}

CollaborationController::~CollaborationController() = default;

}  // namespace collaboration
