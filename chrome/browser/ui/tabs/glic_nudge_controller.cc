// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_nudge_controller.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "components/prefs/pref_service.h"
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

void GlicNudgeController::UpdateNudgeLabel(
    content::WebContents* web_contents,
    const std::string& nudge_label,
    std::optional<GlicNudgeActivity> activity,
    GlicNudgeActivityCallback callback) {
  auto* const tab_interface =
      browser_window_interface_->GetActiveTabInterface();
  if (tab_interface->GetContents() != web_contents) {
    callback.Run(GlicNudgeActivity::kNudgeNotShownWebContents);
    return;
  }

  nudge_activity_callback_ = callback;
  PrefService* const pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  if (pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
    for (auto& observer : observers_) {
      observer.OnTriggerGlicNudgeUI(nudge_label);
    }
  }

  if (nudge_label.empty()) {
    CHECK(activity);
    OnNudgeActivity(*activity);
  }
}

void GlicNudgeController::OnNudgeActivity(GlicNudgeActivity activity) {
  if (!nudge_activity_callback_) {
    return;
  }
  switch (activity) {
    case GlicNudgeActivity::kNudgeShown:
      nudge_activity_callback_.Run(GlicNudgeActivity::kNudgeShown);
      break;
    case GlicNudgeActivity::kNudgeClicked:
    case GlicNudgeActivity::kNudgeDismissed:
    case GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
    case GlicNudgeActivity::kNudgeIgnoredNavigation:
      nudge_activity_callback_.Run(activity);
      nudge_activity_callback_.Reset();
      break;
    case GlicNudgeActivity::kNudgeNotShownWebContents:
      nudge_activity_callback_.Reset();
      break;
  }
}

void GlicNudgeController::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  for (auto& observer : observers_) {
    observer.OnTriggerGlicNudgeUI(std::string());
  }
  OnNudgeActivity(tabs::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
}

}  // namespace tabs
