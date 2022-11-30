// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"

PasswordChangeRunController::Model::Model() = default;
PasswordChangeRunController::Model::Model(Model&& other) = default;
PasswordChangeRunController::Model::Model(Model& other) = default;
PasswordChangeRunController::Model&
PasswordChangeRunController::Model::operator=(
    const PasswordChangeRunController::Model& other) = default;
PasswordChangeRunController::Model&
PasswordChangeRunController::Model::operator=(
    PasswordChangeRunController::Model&& other) = default;

PasswordChangeRunController::Model::~Model() = default;
