// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"

namespace omnibox {

namespace {

bool g_create_popup_webui_on_demand = false;

}  // namespace

bool ShouldCreatePopupWebUIOnDemand() {
  return g_create_popup_webui_on_demand;
}

base::AutoReset<bool> SetCreatePopupWebUIOnDemandForTesting(bool on_demand) {
  return base::AutoReset<bool>(&g_create_popup_webui_on_demand, on_demand);
}

}  // namespace omnibox
