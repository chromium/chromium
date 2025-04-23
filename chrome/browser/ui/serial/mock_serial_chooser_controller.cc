// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/mock_serial_chooser_controller.h"

#include <string>

#include "components/permissions/chooser_controller.h"

MockSerialChooserController::MockSerialChooserController(std::u16string title)
    : permissions::ChooserController(title) {}
MockSerialChooserController::~MockSerialChooserController() = default;
