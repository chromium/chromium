// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"

namespace new_tab_footer {

namespace {
// Adding any new conditions that show the footer on the 1P NTP should also
// update the visibility confition for the `Customize Chrome` buttons amd theme
// attribution on the NTP.
// LINT.IfChange(WillShowFooter)
bool WillShowFooter(const GURL& url,
                    content::WebContents* web_contents,
                    Profile* profile) {
  const bool will_show_extension =
      ntp_footer::IsExtensionNtp(url, profile) &&
      profile->GetPrefs()->GetBoolean(
          prefs::kNTPFooterExtensionAttributionEnabled) &&
      profile->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible);
  return will_show_extension ||
         ntp_footer::WillShowManagementNotice(url, web_contents, profile);
}
// LINT.ThenChange(chrome/browser/ui/webui/new_tab_page/new_tab_footer_handler.cc:OnFooterVisibilityUpdated)
}  // namespace

NewTabFooterController::NewTabFooterController(BrowserWindowInterface* browser,
                                               NewTabFooterWebView* footer)
    : browser_(browser), footer_(footer) {
  profile_ = browser_->GetProfile();
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpFooterVisible,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibility,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
  pref_change_registrar_.Add(
      prefs::kNTPFooterExtensionAttributionEnabled,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibility,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  local_state_pref_change_registrar_.Init(g_browser_process->local_state());
  local_state_pref_change_registrar_.Add(
      prefs::kNTPFooterManagementNoticeEnabled,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibility,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
#endif

  tab_activation_subscription_ = browser_->RegisterActiveTabDidChange(
      base::BindRepeating(&NewTabFooterController::OnActiveTabChanged,
                          weak_factory_.GetWeakPtr()));
}

NewTabFooterController::~NewTabFooterController() = default;

void NewTabFooterController::TearDown() {
  tab_activation_subscription_ = base::CallbackListSubscription();
  pref_change_registrar_.Reset();
  local_state_pref_change_registrar_.Reset();
  profile_ = nullptr;
  footer_ = nullptr;
  browser_ = nullptr;
}

void NewTabFooterController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  UpdateFooterVisibility(/*log_on_load_metric=*/true);
}

void NewTabFooterController::UpdateFooterVisibility(bool log_on_load_metric) {
  base::TimeTicks load_start_timestamp = base::TimeTicks::Now();
  // TODO(crbug.com/4438803): Support SideBySide. Currently, when it is enabled,
  // footer_ will have no value.
  if (!footer_) {
    return;
  }

  GURL url = web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  if (url.is_empty()) {
    url = web_contents()->GetController().GetVisibleEntry()->GetURL();
  }

  const bool show = WillShowFooter(url, web_contents(), profile_);
  if (show) {
    footer_->ShowUI(load_start_timestamp, url);
  } else {
    footer_->CloseUI();
  }

  if (ntp_footer::IsNtp(url, web_contents(), profile_) && log_on_load_metric) {
    base::UmaHistogramBoolean("NewTabPage.Footer.VisibleOnLoad", show);
  }
}

void NewTabFooterController::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  Observe(browser->GetActiveTabInterface()->GetContents());
  UpdateFooterVisibility(/*log_on_load_metric=*/true);
}

}  // namespace new_tab_footer
