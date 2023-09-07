// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/status_area_internals/status_area_internals_handler.h"
#include <utility>

#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_ui.h"

namespace ash {

// static
const char StatusAreaInternalsHandler::kToggleIme[] = "toggleIme";
const char StatusAreaInternalsHandler::kTogglePalette[] = "togglePalette";
const char StatusAreaInternalsHandler::kTriggerPrivacyIndicators[] =
    "triggerPrivacyIndicators";

StatusAreaInternalsHandler::StatusAreaInternalsHandler(
    mojo::PendingReceiver<mojom::status_area_internals::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

StatusAreaInternalsHandler::~StatusAreaInternalsHandler() = default;

void StatusAreaInternalsHandler::ToggleImeTray(bool visible) {
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(/*show=*/visible);
}

void StatusAreaInternalsHandler::TogglePaletteTray(bool visible) {
  if (visible) {
    stylus_utils::SetHasStylusInputForTesting();
  } else {
    stylus_utils::SetNoStylusInputForTesting();
  }

  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    root_window_controller->GetStatusAreaWidget()
        ->palette_tray()
        ->SetDisplayHasStylusForTesting();
  }
}

void StatusAreaInternalsHandler::TriggerPrivacyIndicators(
    const std::string& app_id,
    const std::string& app_name,
    bool is_camera_used,
    bool is_microphone_used) {
  PrivacyIndicatorsController::Get()->UpdatePrivacyIndicators(
      app_id, base::UTF8ToUTF16(app_name), is_camera_used, is_microphone_used,
      base::MakeRefCounted<PrivacyIndicatorsNotificationDelegate>(),
      PrivacyIndicatorsSource::kApps);
}

}  // namespace ash
