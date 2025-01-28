// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_nudge_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "content/public/browser/web_contents.h"

namespace tabs {

GlicNudgeController::GlicNudgeController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface) {
  browser_subscriptions_.push_back(
      browser_window_interface->RegisterActiveTabDidChange(base::BindRepeating(
          &GlicNudgeController::OnActiveTabChanged, base::Unretained(this))));
}

GlicNudgeController::~GlicNudgeController() = default;

bool GlicNudgeController::GlicNudgeCriteriaMet() {
  return false;
}

void GlicNudgeController::UpdateNudgeLabel(content::WebContents* web_contents,
                                           const std::string& nudge_label) {
  auto* const tab_interface =
      browser_window_interface_->GetActiveTabInterface();
  if (tab_interface->GetContents() != web_contents) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTriggerGlicNudgeUI(nudge_label);
  }
}

void GlicNudgeController::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  for (auto& observer : observers_) {
    observer.OnTriggerGlicNudgeUI(std::string());
  }
}

}  // namespace tabs
