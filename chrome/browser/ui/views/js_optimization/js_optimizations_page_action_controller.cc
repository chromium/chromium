// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/js_optimization/js_optimizations_page_action_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"

JsOptimizationsPageActionController::JsOptimizationsPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : tabs::ContentsObservingTabFeature(tab_interface),
      page_action_controller_(page_action_controller) {
  UpdateIconVisibility();
}

JsOptimizationsPageActionController::~JsOptimizationsPageActionController() =
    default;

void JsOptimizationsPageActionController::PrimaryPageChanged(
    content::Page& page) {
  UpdateIconVisibility();
}

void JsOptimizationsPageActionController::UpdateIconVisibility() {
  if (site_protection::AreV8OptimizationsDisabled(web_contents()) == true) {
    page_action_controller_->Show(kActionShowJsOptimizationsIcon);
  } else {
    page_action_controller_->Hide(kActionShowJsOptimizationsIcon);
  }
}
