// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_nudge_controller.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#endif

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
  // CanShowCallToAction check.
  if (!nudge_label.empty() &&
      !browser_window_interface_->CanShowCallToAction()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI));
    return;
  }

  if (activity &&
      activity == tabs::GlicNudgeActivity::
                      kNudgeIgnoredOpenedContextualTasksSidePanel &&
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
      delegate_->OnTriggerGlicNudgeUI(nudge_label);
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
      // We should only have a GlicNudgeController if the ENABLE_GLIC buildflag
      // is set. However, since we don't prevent it by having #if's across the
      // various places the class is referenced (which would be noisy), it's
      // possible to have this class built even when that buildflag isn't set,
      // so we'll conditionally compile this next section.
#if BUILDFLAG(ENABLE_GLIC)
      auto* profile = browser_window_interface_->GetProfile();
      auto* glic_service =
          glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
      glic_service->TryPreloadFre(glic::GlicPrewarmingFreSource::kNudge);
#endif
      nudge_activity_callback_.Run(GlicNudgeActivity::kNudgeShown);
      scoped_window_call_to_action_ptr =
          browser_window_interface_->ShowCallToAction();
      break;
    }
    case GlicNudgeActivity::kNudgeClicked:
    case GlicNudgeActivity::kNudgeDismissed:
    case GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
    case GlicNudgeActivity::kNudgeIgnoredNavigation:
    case GlicNudgeActivity::kNudgeIgnoredOpenedContextualTasksSidePanel:
      nudge_activity_callback_.Run(activity);
      nudge_activity_callback_.Reset();
      scoped_window_call_to_action_ptr.reset();

      break;
    case GlicNudgeActivity::kNudgeNotShownWebContents:
    case GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      scoped_window_call_to_action_ptr.reset();
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
