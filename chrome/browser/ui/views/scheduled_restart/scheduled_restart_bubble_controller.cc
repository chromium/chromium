// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_controller.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_bubble_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/common/management/management_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace {

bool AreScheduledRestartsEnabled() {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kSimulateUpgrade) &&
      !cmd_line->HasSwitch(switches::kSimulateOutdated) &&
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
    return false;
  }
  return base::FeatureList::IsEnabled(features::kScheduledRestart);
}

}  // namespace

namespace scheduled_restart {

DEFINE_USER_DATA(ScheduledRestartBubbleController);

// static
ScheduledRestartBubbleController* ScheduledRestartBubbleController::From(
    BrowserProcess* browser_process) {
  return browser_process ? Get(browser_process->GetUnownedUserDataHost())
                         : nullptr;
}

// static
void ScheduledRestartBubbleController::MaybeShowNTPNudge(
    content::WebContents* web_contents) {
  if (auto* controller =
          ScheduledRestartBubbleController::From(g_browser_process)) {
    controller->MaybeShowNudgeForWebContents(web_contents);
  }
}

ScheduledRestartBubbleController::ScheduledRestartBubbleController() {
  if (g_browser_process &&
      !ScheduledRestartBubbleController::From(g_browser_process)) {
    scoped_unowned_user_data_.emplace(
        g_browser_process->GetUnownedUserDataHost(), *this);
  }
}

ScheduledRestartBubbleController::~ScheduledRestartBubbleController() = default;

void ScheduledRestartBubbleController::MaybeShowNudgeForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  // 1. Tab & WebContents Checks:
  // Must be visible foreground tab.
  if (web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    VLOG(1) << "ScheduledRestartBubbleController: Ignored non-visible tab.";
    return;
  }

  // Ignore tabs opened with openers (e.g. popups, script-initiated tabs).
  if (web_contents->HasOpener()) {
    VLOG(1) << "ScheduledRestartBubbleController: Ignored tab insertion with "
               "opener.";
    return;
  }

  // Must be a fresh new tab without prior history.
  if (web_contents->GetController().GetEntryCount() > 1) {
    VLOG(1) << "ScheduledRestartBubbleController: Ignored navigated tab with "
               "history.";
    return;
  }

  // Ignore tabs during active session restore or restored from session state.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile && SessionRestore::IsRestoring(profile)) {
    VLOG(1) << "ScheduledRestartBubbleController: Ignored tab during session "
               "restore.";
    return;
  }

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->IsRestored()) {
    VLOG(1) << "ScheduledRestartBubbleController: Ignored restored tab.";
    return;
  }

  // 2. Feature & Policy Checks:
  if (!AreScheduledRestartsEnabled()) {
    return;
  }

  auto* srm = GetScheduledRestartManager();
  if (!srm || srm->is_scheduled()) {
    VLOG(1) << "ScheduledRestartBubbleController: Ignored - Restart already "
               "scheduled or manager unavailable.";
    return;
  }

  if (!srm->ShouldShowNudge()) {
    return;
  }

  if (is_bubble_showing()) {
    VLOG(1) << "ScheduledRestartBubbleController: Bubble already showing.";
    return;
  }

  // 3. UI Presentation:
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser) {
    VLOG(1) << "ScheduledRestartBubbleController: No valid browser window "
               "found.";
    return;
  }

  views::Widget* widget = ShowBubble(browser);
  if (widget) {
    bubble_widget_observation_.Observe(widget);
    srm->RecordNudgeShown();
    VLOG(1) << "ScheduledRestartBubbleController: Showing Scheduled Restart "
               "bubble.";
  }
}

// Shows the restart nudge bubble. Uses RelaunchRecommendedBubbleView as an
// interim prompt until ScheduledRestartBubbleView is landed in follow-up CL.
views::Widget* ScheduledRestartBubbleController::ShowBubble(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  auto* srm = GetScheduledRestartManager();
  UpgradeDetector* detector = srm ? srm->upgrade_detector() : nullptr;
  return RelaunchRecommendedBubbleView::ShowBubble(
      browser, detector ? detector->upgrade_detected_time() : base::Time::Now(),
      /*on_accept=*/base::BindRepeating(&chrome::AttemptRelaunch));
}

void ScheduledRestartBubbleController::OnWidgetDestroying(
    views::Widget* widget) {
  bubble_widget_observation_.Reset();
}

ScheduledRestartManager*
ScheduledRestartBubbleController::GetScheduledRestartManager() const {
  if (scheduled_restart_manager_for_testing_) {
    return scheduled_restart_manager_for_testing_;
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return g_browser_process && g_browser_process->GetFeatures()
             ? g_browser_process->GetFeatures()->scheduled_restart_manager()
             : nullptr;
#else
  return nullptr;
#endif
}

}  // namespace scheduled_restart
