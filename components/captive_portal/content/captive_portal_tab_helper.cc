// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/content/captive_portal_tab_helper.h"

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/captive_portal/content/captive_portal_login_detector.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/ssl/ssl_info.h"

namespace captive_portal {

CaptivePortalTabHelper::CaptivePortalTabHelper(
    content::WebContents* web_contents,
    CaptivePortalService* captive_portal_service,
    const CaptivePortalTabReloader::OpenLoginTabCallback&
        open_login_tab_callback)

    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CaptivePortalTabHelper>(*web_contents),
      tab_reloader_(new CaptivePortalTabReloader(captive_portal_service,
                                                 web_contents,
                                                 open_login_tab_callback)),

      login_detector_(new CaptivePortalLoginDetector(captive_portal_service)),
      subscription_(captive_portal_service->RegisterCallback(
          base::BindRepeating(&CaptivePortalTabHelper::Observe,
                              base::Unretained(this)))) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CaptivePortalTabHelper::~CaptivePortalTabHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CaptivePortalTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // TODO(clamy): The root cause behind crbug.com/704892 is known.
  // Remove this code if it is never reached until ~ 2017-July-20.
  if (navigation_handle == navigation_handle_)
    base::debug::DumpWithoutCrashing();

  bool was_tracking_navigation = !!navigation_handle_;
  navigation_handle_ = navigation_handle;

  // Always track the latest navigation. If a navigation was already tracked,
  // and it committed (either the navigation proper or an error page), it is
  // safe to start tracking the new navigation. Otherwise simulate an abort
  // before reporting the start of the new navigation.
  if (was_tracking_navigation)
    tab_reloader_->OnAbort();

  tab_reloader_->OnLoadStart(
      navigation_handle->GetURL().SchemeIsCryptographic());
}

void CaptivePortalTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (navigation_handle != navigation_handle_)
    return;
  DCHECK(navigation_handle->IsInPrimaryMainFrame());
  tab_reloader_->OnRedirect(
      navigation_handle->GetURL().SchemeIsCryptographic());
}

void CaptivePortalTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Exclude non-primary frame and subframe navigations.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // Exclude same-document navigations and aborted navigations that were not
  // being tracked.
  if (navigation_handle_ != navigation_handle &&
      (!navigation_handle->HasCommitted() ||
       navigation_handle->IsSameDocument())) {
    return;
  }

  bool need_to_simulate_start = navigation_handle_ != navigation_handle;
  bool need_to_simulate_previous_abort =
      need_to_simulate_start && !!navigation_handle_;
  navigation_handle_ = nullptr;

  if (need_to_simulate_previous_abort)
    tab_reloader_->OnAbort();

  if (need_to_simulate_start) {
    tab_reloader_->OnLoadStart(
        navigation_handle->GetURL().SchemeIsCryptographic());
  }

  if (navigation_handle->HasCommitted()) {
    tab_reloader_->OnLoadCommitted(navigation_handle->GetNetErrorCode(),
                                   navigation_handle->GetResolveErrorInfo());
  } else {
    tab_reloader_->OnAbort();
  }
}

void CaptivePortalTabHelper::DidStopLoading() {
  login_detector_->OnStoppedLoading();
}

void CaptivePortalTabHelper::Observe(
    const CaptivePortalService::Results& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnCaptivePortalResults(results.previous_result, results.result);
}

void CaptivePortalTabHelper::OnSSLCertError(const net::SSLInfo& ssl_info) {
  tab_reloader_->OnSSLCertError(ssl_info);
}

bool CaptivePortalTabHelper::IsLoginTab() const {
  return login_detector_->is_login_tab();
}

void CaptivePortalTabHelper::OnCaptivePortalResults(
    CaptivePortalResult previous_result,
    CaptivePortalResult result) {
  tab_reloader_->OnCaptivePortalResults(previous_result, result);
  login_detector_->OnCaptivePortalResults(previous_result, result);
}

void CaptivePortalTabHelper::SetIsLoginTab() {
  login_detector_->SetIsLoginTab();
}

void CaptivePortalTabHelper::SetTabReloaderForTest(
    CaptivePortalTabReloader* tab_reloader) {
  tab_reloader_.reset(tab_reloader);
}

CaptivePortalTabReloader* CaptivePortalTabHelper::GetTabReloaderForTest() {
  return tab_reloader_.get();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CaptivePortalTabHelper);

}  // namespace captive_portal
