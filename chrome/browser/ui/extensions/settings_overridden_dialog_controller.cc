// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"

#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"

SettingsOverriddenDialogController::ShowParams::ShowParams() = default;
SettingsOverriddenDialogController::ShowParams::~ShowParams() = default;
SettingsOverriddenDialogController::ShowParams::ShowParams(
    const SettingsOverriddenDialogController::ShowParams& params) = default;

SettingsOverriddenDialogController::ShowParams::ShowParams(
    std::u16string dialog_title,
    std::u16string dialog_message,
    const gfx::VectorIcon* icon)
    : dialog_title(std::move(dialog_title)),
      message(std::move(dialog_message)),
      icon(icon) {}
