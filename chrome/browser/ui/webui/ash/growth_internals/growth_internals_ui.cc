// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/growth_internals/growth_internals_ui.h"

#include <sstream>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/containers/fixed_flat_set.h"
#include "base/memory/ref_counted_memory.h"
#include "chromeos/ash/components/growth/campaigns_constants.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace ash {

bool GrowthInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsGrowthInternalsEnabled();
}

GrowthInternalsUI::GrowthInternalsUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {}

GrowthInternalsUI::~GrowthInternalsUI() = default;

}  // namespace ash
