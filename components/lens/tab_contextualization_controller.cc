// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/tab_contextualization_controller.h"

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace lens {

DEFINE_USER_DATA(TabContextualizationController);

TabContextualizationController::TabContextualizationController(
    tabs::TabInterface* tab)
    : scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

TabContextualizationController::~TabContextualizationController() = default;

TabContextualizationController* TabContextualizationController::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// TODO(crbug.com/439595898): Get contextual page content.
void TabContextualizationController::GetPageContext(
    GetPageContextCallback callback) {}

// TODO(crbug.com/439597165): Check tab eligibility
void TabContextualizationController::GetInitialPageContextEligibility(
    GetPageContextEligibilityCallback callback) {}

// TODO(crbug.com/439597165): Check tab eligibility.
bool TabContextualizationController::GetCurrentPageContextEligibility() {
  return false;
}

}  // namespace lens
