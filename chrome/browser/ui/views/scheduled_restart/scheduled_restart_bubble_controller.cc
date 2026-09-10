// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_controller.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_view.h"
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
    return;
  }

  // Ignore tabs opened with openers (e.g. popups, script-initiated tabs).
  if (web_contents->HasOpener()) {
    return;
  }

  // Must be a fresh new tab without prior history.
  if (web_contents->GetController().GetEntryCount() > 1) {
    return;
  }

  // Ignore tabs during active session restore or restored from session state.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile && SessionRestore::IsRestoring(profile)) {
    return;
  }

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->IsRestored()) {
    return;
  }

  // 2. Feature & Policy Checks:
  if (!AreScheduledRestartsEnabled()) {
    return;
  }

  auto* srm = GetScheduledRestartManager();
  if (!srm || srm->is_scheduled()) {
    return;
  }

  if (!srm->ShouldShowNudge()) {
    return;
  }

  if (is_bubble_showing()) {
    return;
  }

  // 3. UI Presentation:
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser) {
    return;
  }

  bubble_widget_ = ShowBubble(
      browser, base::BindOnce(&ScheduledRestartBubbleController::OnBubbleClosed,
                              base::Unretained(this)));
  if (bubble_widget_) {
    srm->RecordNudgeShown();
  }
}

// Shows the restart nudge bubble anchored to the browser's App Menu.
std::unique_ptr<views::Widget> ScheduledRestartBubbleController::ShowBubble(
    BrowserWindowInterface* browser,
    views::Widget::ClosedCallback on_close) {
  if (!browser) {
    return nullptr;
  }
  return ScheduledRestartBubbleView::ShowBubble(browser, std::move(on_close));
}

void ScheduledRestartBubbleController::OnBubbleClosed(
    views::Widget::ClosedReason reason) {
  if (bubble_widget_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(bubble_widget_));
  }
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
