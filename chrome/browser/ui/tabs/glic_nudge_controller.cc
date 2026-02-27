// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_nudge_controller.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"
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
    std::optional<std::string> prompt_suggestion,
    std::optional<GlicNudgeActivity> activity,
    GlicNudgeActivityCallback callback) {
  auto* const tab_interface =
      browser_window_interface_->GetActiveTabInterface();
  if (tab_interface->GetContents() != web_contents) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWebContents));
    return;
  }
  // Empty nudge labels close the nudge, allow those to bypass the
  // CanAcquireLock check.
  if (!nudge_label.empty() &&
      !CallToActionLock::From(browser_window_interface_)->CanAcquireLock()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI));
    return;
  }

  if (activity &&
      (activity == tabs::GlicNudgeActivity::
                       kNudgeIgnoredOpenedContextualTasksSidePanel ||
       activity == tabs::GlicNudgeActivity::
                       kNudgeIgnoredOmniboxContextMenuInteraction) &&
      delegate_ && delegate_->GetIsShowingGlicNudge()) {
    delegate_->OnHideGlicNudgeUI();
    OnNudgeActivity(*activity);
    return;
  }

  nudge_activity_callback_ = callback;
  PrefService* const pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  if (pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
    if (delegate_) {
      if (nudge_label.empty() && delegate_->GetIsShowingGlicNudge()) {
        delegate_->OnHideGlicNudgeUI();
      } else {
        delegate_->OnTriggerGlicNudgeUI(nudge_label);
      }
    }
  }

  if (nudge_label.empty()) {
    CHECK(activity);
    OnNudgeActivity(*activity);
  } else {
    OnNudgeActivity(tabs::GlicNudgeActivity::kNudgeShown);
  }

  prompt_suggestion_ = prompt_suggestion;
}

void GlicNudgeController::OnNudgeActivity(GlicNudgeActivity activity) {
  if (!nudge_activity_callback_) {
    return;
  }
  switch (activity) {
    case GlicNudgeActivity::kNudgeShown: {
      auto* profile = browser_window_interface_->GetProfile();
      auto* glic_service =
          glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
      glic_service->TryPreloadFre(glic::GlicPrewarmingFreSource::kNudge);
      nudge_activity_callback_.Run(GlicNudgeActivity::kNudgeShown);
      scoped_call_to_action_lock_ =
          CallToActionLock::From(browser_window_interface_)->AcquireLock();
      break;
    }
    case GlicNudgeActivity::kNudgeClicked:
    case GlicNudgeActivity::kNudgeDismissed:
    case GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
    case GlicNudgeActivity::kNudgeIgnoredNavigation:
    case GlicNudgeActivity::kNudgeIgnoredOpenedContextualTasksSidePanel:
    case GlicNudgeActivity::kNudgeIgnoredOmniboxContextMenuInteraction:
      nudge_activity_callback_.Run(activity);
      nudge_activity_callback_.Reset();
      scoped_call_to_action_lock_.reset();

      break;
    case GlicNudgeActivity::kNudgeNotShownWebContents:
    case GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      scoped_call_to_action_lock_.reset();
      nudge_activity_callback_.Reset();
      break;
  }
}

void GlicNudgeController::SetNudgeActivityCallbackForTesting() {
  nudge_activity_callback_ = base::DoNothing();
}

void GlicNudgeController::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  if (delegate_ && delegate_->GetIsShowingGlicNudge()) {
    delegate_->OnHideGlicNudgeUI();
    OnNudgeActivity(tabs::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
  }
}

}  // namespace tabs
