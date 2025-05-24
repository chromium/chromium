// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"

namespace new_tab_footer {

namespace {
bool IsNtp(const GURL& url,
           content::WebContents* web_contents,
           Profile* profile) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    entry = web_contents->GetController().GetVisibleEntry();
  }
  return NewTabUI::IsNewTab(url) || NewTabPageUI::IsNewTabPageOrigin(url) ||
         NewTabPageThirdPartyUI::IsNewTabPageOrigin(url) ||
         search::NavEntryIsInstantNTP(web_contents, entry) ||
         ntp_footer::IsExtensionNtp(url, profile);
}
}  // namespace

NewTabFooterController::NewTabFooterController(BrowserWindowInterface* browser,
                                               NewTabFooterWebView* footer)
    : browser_(browser), footer_(footer) {
  profile_ = browser_->GetProfile();
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpFooterVisible,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibility,
                          weak_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      prefs::kNTPFooterExtensionAttributionEnabled,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibility,
                          weak_factory_.GetWeakPtr()));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  local_state_pref_change_registrar_.Init(g_browser_process->local_state());
  local_state_pref_change_registrar_.Add(
      prefs::kNTPFooterManagementNoticeEnabled,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibility,
                          weak_factory_.GetWeakPtr()));
#endif

  tab_activation_subscription_subscription_ =
      browser_->RegisterActiveTabDidChange(
          base::BindRepeating(&NewTabFooterController::OnActiveTabChanged,
                              weak_factory_.GetWeakPtr()));
}

NewTabFooterController::~NewTabFooterController() = default;

void NewTabFooterController::TearDown() {
  pref_change_registrar_.Reset();
  tab_activation_subscription_subscription_ = base::CallbackListSubscription();
  footer_ = nullptr;
  browser_ = nullptr;
  profile_ = nullptr;
}

void NewTabFooterController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
    UpdateFooterVisibility();
}

void NewTabFooterController::UpdateFooterVisibility() {
  // TODO(crbug.com/4438803): Support SideBySide. Currently, when it is enabled,
  // footer_ will have no value.
  if (!footer_) {
    return;
  }

  GURL url = web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  if (url.is_empty()) {
    url = web_contents()->GetController().GetVisibleEntry()->GetURL();
  }

  bool managed_ntp =
      IsNtp(url, web_contents(), profile_) &&
      enterprise_util::CanShowEnterpriseBadgingForNTPFooter(profile_);
  bool show = managed_ntp ||
              (profile_->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible) &&
               ntp_footer::CanShowExtensionFooter(url, profile_));
  if (show) {
    footer_->ShowUI();
  } else {
    footer_->CloseUI();
  }
}

void NewTabFooterController::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  Observe(browser->GetActiveTabInterface()->GetContents());
  UpdateFooterVisibility();
}

}  // namespace new_tab_footer
