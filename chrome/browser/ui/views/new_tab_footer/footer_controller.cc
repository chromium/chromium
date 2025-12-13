// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller_observer.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace new_tab_footer {

NewTabFooterController::NewTabFooterController(
    Profile* profile,
    std::vector<ContentsContainerView*> contents_container_views)
    : profile_(profile) {
  for (ContentsContainerView* contents_container_view :
       contents_container_views) {
    footer_controllers_.push_back(std::make_unique<ContentsViewFooterCotroller>(
        this, contents_container_view));
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpFooterVisible,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibilities,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
  pref_change_registrar_.Add(
      prefs::kNTPFooterExtensionAttributionEnabled,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibilities,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  local_state_pref_change_registrar_.Init(g_browser_process->local_state());
  local_state_pref_change_registrar_.Add(
      prefs::kNTPFooterManagementNoticeEnabled,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibilities,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
  local_state_pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabelForBrowser,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibilities,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
  local_state_pref_change_registrar_.Add(
      prefs::kEnterpriseLogoUrlForBrowser,
      base::BindRepeating(&NewTabFooterController::UpdateFooterVisibilities,
                          weak_factory_.GetWeakPtr(),
                          /*log_on_load_metric=*/false));
#endif
}

NewTabFooterController::~NewTabFooterController() = default;

void NewTabFooterController::TearDown() {
  pref_change_registrar_.Reset();
  local_state_pref_change_registrar_.Reset();
  profile_ = nullptr;
  footer_controllers_.clear();
}

bool NewTabFooterController::GetFooterVisible(
    content::WebContents* contents) const {
  for (const auto& footer_controller : footer_controllers_) {
    if (footer_controller->web_contents() == contents) {
      return footer_controller->GetFooterVisible();
    }
  }
  return false;
}

void NewTabFooterController::AddObserver(
    NewTabFooterControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void NewTabFooterController::RemoveObserver(
    NewTabFooterControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

NewTabFooterController::ContentsViewFooterCotroller::
    ContentsViewFooterCotroller(NewTabFooterController* owner,
                                ContentsContainerView* contents_container_view)
    : owner_(owner), footer_(contents_container_view->new_tab_footer_view()) {
  ContentsWebView* contents_web_view = contents_container_view->contents_view();
  web_contents_attached_subscription_ =
      contents_web_view->AddWebContentsAttachedCallback(base::BindRepeating(
          &ContentsViewFooterCotroller::OnWebContentsAttached,
          base::Unretained(this)));
  web_contents_detached_subscription_ =
      contents_web_view->AddWebContentsDetachedCallback(base::BindRepeating(
          &ContentsViewFooterCotroller::OnWebContentsDetached,
          base::Unretained(this)));
  OnWebContentsAttached(contents_web_view);
}

void NewTabFooterController::ContentsViewFooterCotroller::OnWebContentsAttached(
    views::WebView* web_view) {
  Observe(web_view->web_contents());
  UpdateFooterVisibility(/*log_on_load_metric=*/true);
}

void NewTabFooterController::ContentsViewFooterCotroller::OnWebContentsDetached(
    views::WebView* web_view) {
  Observe(nullptr);
}

void NewTabFooterController::ContentsViewFooterCotroller::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    UpdateFooterVisibility(/*log_on_load_metric=*/true);
  }
}

void NewTabFooterController::ContentsViewFooterCotroller::
    UpdateFooterVisibility(bool log_on_load_metric) {
  if (!web_contents()) {
    return;
  }
  base::TimeTicks load_start_timestamp = base::TimeTicks::Now();

  GURL url = web_contents()->GetController().GetLastCommittedEntry()->GetURL();
  if (url.is_empty()) {
    url = web_contents()->GetController().GetVisibleEntry()->GetURL();
  }

  const bool show_managed = ShouldShowManagedFooter(url);
  const bool show_extension = ShouldShowExtensionFooter(url);
  const bool show = show_managed || show_extension;
  if (show) {
    footer_->ShowUI(load_start_timestamp, url);
  } else {
    footer_->CloseUI();
  }
  owner_->observers_.Notify(
      &NewTabFooterControllerObserver::OnFooterVisibilityUpdated, show);

  if (!log_on_load_metric) {
    return;
  }

  if (ntp_footer::IsNtp(url, web_contents(), owner_->profile_)) {
    base::UmaHistogramBoolean("NewTabPage.Footer.VisibleOnLoad", show);
  }
  if (show_managed) {
    base::UmaHistogramEnumeration("NewTabPage.Footer.NoticeItem",
                                  FooterNoticeItem::kManagementNotice);
  }
  if (show_extension) {
    base::UmaHistogramEnumeration("NewTabPage.Footer.NoticeItem",
                                  FooterNoticeItem::kExtensionAttribution);
  }
}

bool NewTabFooterController::ContentsViewFooterCotroller::GetFooterVisible() {
  return footer_ && footer_->GetVisible();
}

bool NewTabFooterController::ContentsViewFooterCotroller::
    ShouldSkipForErrorPage() const {
  if (owner_->skip_error_page_check_for_testing_) {
    return false;
  }
  return web_contents()->GetSiteInstance()->GetSiteURL().SchemeIs(
      content::kChromeErrorScheme);
}

bool NewTabFooterController::ContentsViewFooterCotroller::
    ShouldShowManagedFooter(const GURL& url) {
  if (ShouldSkipForErrorPage()) {
    return false;
  }

  enterprise_util::BrowserManagementNoticeState state =
      enterprise_util::GetManagementNoticeStateForNTPFooter(owner_->profile_);
  switch (state) {
    case enterprise_util::BrowserManagementNoticeState::kNotApplicable:
    case enterprise_util::BrowserManagementNoticeState::kDisabled:
      return false;
    case enterprise_util::BrowserManagementNoticeState::kEnabled:
    case enterprise_util::BrowserManagementNoticeState::kEnabledByPolicy:
      return ntp_footer::IsNtp(url, web_contents(), owner_->profile_);
  }
}

bool NewTabFooterController::ContentsViewFooterCotroller::
    ShouldShowExtensionFooter(const GURL& url) {
  if (ShouldSkipForErrorPage()) {
    return false;
  }

  return ntp_footer::IsExtensionNtp(url, owner_->profile_) &&
         owner_->profile_->GetPrefs()->GetBoolean(
             prefs::kNTPFooterExtensionAttributionEnabled) &&
         owner_->profile_->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible);
}

void NewTabFooterController::UpdateFooterVisibilities(bool log_on_load_metric) {
  for (const auto& footer_controller : footer_controllers_) {
    footer_controller->UpdateFooterVisibility(log_on_load_metric);
  }
}

}  // namespace new_tab_footer
