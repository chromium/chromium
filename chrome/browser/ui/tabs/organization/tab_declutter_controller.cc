// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"

#include "content/public/browser/browser_context.h"

namespace tabs {

TabDeclutterController::TabDeclutterController(
    TabStripModel* tab_strip_model,
    content::BrowserContext* browser_context)
    : tab_strip_model_(tab_strip_model), browser_context_(browser_context) {}

TabDeclutterController::~TabDeclutterController() {}

void TabDeclutterController::ProcessInactiveTabs() {
  // TODO(shibalik): Implement the logic to compute inactive tabs in the
  // tabstrip.
}

bool TabDeclutterController::DeclutterNudgeCriteriaMet() {
  // TODO(shibalik): Implement whether the declutter nudge can be shown.
  return false;
}

}  // namespace tabs
