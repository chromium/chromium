// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_tab_helper.h"

#include <memory>
#include <string>

#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"

namespace web_app {

void WebAppTabHelper::CreateForWebContents(content::WebContents* contents) {
  DCHECK(contents);
  if (!FromWebContents(contents)) {
    contents->SetUserData(UserDataKey(),
                          std::make_unique<WebAppTabHelper>(contents));
  }
}

WebAppTabHelper::WebAppTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<WebAppTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  DCHECK(provider_);
  observation_.Observe(&provider_->install_manager());
  SetAppId(
      FindAppIdWithUrlInScope(web_contents->GetSiteInstance()->GetSiteURL()));
}

WebAppTabHelper::~WebAppTabHelper() = default;

const AppId& WebAppTabHelper::GetAppId() const {
  return app_id_;
}

const base::UnguessableToken& WebAppTabHelper::GetAudioFocusGroupIdForTesting()
    const {
  return audio_focus_group_id_;
}

WebAppLaunchQueue& WebAppTabHelper::EnsureLaunchQueue() {
  if (!launch_queue_) {
    launch_queue_ = std::make_unique<WebAppLaunchQueue>(web_contents(),
                                                        provider_->registrar());
  }
  return *launch_queue_;
}

void WebAppTabHelper::SetAppId(const AppId& app_id) {
  DCHECK(app_id.empty() || provider_->registrar().IsInstalled(app_id));
  if (app_id_ == app_id)
    return;

  AppId previous_app_id = app_id_;
  app_id_ = app_id;

  OnAssociatedAppChanged(previous_app_id, app_id_);
}

void WebAppTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    const GURL& url = navigation_handle->GetURL();
    const AppId app_id = FindAppIdWithUrlInScope(url);
    SetAppId(app_id);
  }

  // If navigating to a System Web App (including navigation in sub frames), let
  // SystemWebAppManager perform tab-secific setup for navigations in System Web
  // Apps.
  if (provider_->system_web_app_manager().IsSystemWebApp(GetAppId())) {
    provider_->system_web_app_manager().OnReadyToCommitNavigation(
        GetAppId(), navigation_handle);
  }
}

void WebAppTabHelper::PrimaryPageChanged(content::Page& page) {
  // This method is invoked whenever primary page of a WebContents
  // (WebContents::GetPrimaryPage()) changes to `page`. This happens in one of
  // the following cases:
  // 1) when the current RenderFrameHost in the primary main frame changes after
  //    a navigation.
  // 2) when the current RenderFrameHost in the primary main frame is
  //    reinitialized after a crash.
  // 3) when a cross-document navigation commits in the current RenderFrameHost
  //    of the primary main frame.
  //
  // The new primary page might either be a brand new one (if the committed
  // navigation created a new document in the primary main frame) or an existing
  // one (back-forward cache restore or prerendering activation).
  //
  // This notification is not dispatched for changes of pages in the non-primary
  // frame trees (prerendering, fenced frames) and when the primary page is
  // destroyed (e.g., when closing a tab).
  //
  // See the declaration of WebContentsObserver::PrimaryPageChanged for more
  // information.
  provider_->manifest_update_manager().MaybeUpdate(
      page.GetMainDocument().GetLastCommittedURL(), GetAppId(), web_contents());

  ReinstallPlaceholderAppIfNecessary(
      page.GetMainDocument().GetLastCommittedURL());
}

void WebAppTabHelper::DidCloneToNewWebContents(
    content::WebContents* old_web_contents,
    content::WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a WebAppTabHelper.
  CreateForWebContents(new_web_contents);
  auto* new_tab_helper = FromWebContents(new_web_contents);

  // Clone common state:
  new_tab_helper->SetAppId(GetAppId());
}

bool WebAppTabHelper::IsInAppWindow() const {
  return provider_->ui_manager().IsInAppWindow(web_contents());
}

void WebAppTabHelper::OnWebAppInstalled(const AppId& installed_app_id) {
  // Check if current web_contents url is in scope for the newly installed app.
  AppId app_id = FindAppIdWithUrlInScope(web_contents()->GetURL());
  if (app_id != installed_app_id)
    return;

  SetAppId(app_id);
}

void WebAppTabHelper::OnWebAppWillBeUninstalled(
    const AppId& uninstalled_app_id) {
  if (GetAppId() == uninstalled_app_id)
    ResetAppId();
}

void WebAppTabHelper::OnWebAppInstallManagerDestroyed() {
  observation_.Reset();
  ResetAppId();
}

void WebAppTabHelper::ResetAppId() {
  if (app_id_.empty())
    return;

  AppId previous_app_id = app_id_;
  app_id_.clear();

  OnAssociatedAppChanged(previous_app_id, app_id_);
}

void WebAppTabHelper::OnAssociatedAppChanged(const AppId& previous_app_id,
                                             const AppId& new_app_id) {
  provider_->ui_manager().NotifyOnAssociatedAppChanged(
      web_contents(), previous_app_id, new_app_id);
  UpdateAudioFocusGroupId();
}

void WebAppTabHelper::UpdateAudioFocusGroupId() {
  if (!app_id_.empty() && IsInAppWindow()) {
    audio_focus_group_id_ =
        provider_->audio_focus_id_map().CreateOrGetIdForApp(app_id_);
  } else {
    audio_focus_group_id_ = base::UnguessableToken::Null();
  }

  content::MediaSession::Get(web_contents())
      ->SetAudioFocusGroupId(audio_focus_group_id_);
}

void WebAppTabHelper::ReinstallPlaceholderAppIfNecessary(const GURL& url) {
  provider_->policy_manager().ReinstallPlaceholderAppIfNecessary(url);
}

AppId WebAppTabHelper::FindAppIdWithUrlInScope(const GURL& url) const {
  return provider_->registrar().FindAppWithUrlInScope(url).value_or(AppId());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelper);

}  // namespace web_app
