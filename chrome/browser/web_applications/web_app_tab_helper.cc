// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_tab_helper.h"

#include <memory>
#include <string>

#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
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
    : content::WebContentsObserver(web_contents),
      provider_(WebAppProviderBase::GetProviderBase(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  DCHECK(provider_);
  observer_.Add(&provider_->registrar());
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

bool WebAppTabHelper::HasLoadedNonAboutBlankPage() const {
  return has_loaded_non_about_blank_page_;
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
  if (navigation_handle->IsInMainFrame()) {
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

void WebAppTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  if (!navigation_handle->GetURL().IsAboutBlank())
    has_loaded_non_about_blank_page_ = true;

  is_error_page_ = navigation_handle->IsErrorPage();

  provider_->manifest_update_manager().MaybeUpdate(navigation_handle->GetURL(),
                                                   GetAppId(), web_contents());

  ReinstallPlaceholderAppIfNecessary(navigation_handle->GetURL());
}

void WebAppTabHelper::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host != web_contents()->GetMainFrame())
    return;

  // Don't try and update the expiry time if this is an error page.
  if (is_error_page_)
    return;

  // Don't try and manage file handlers unless this page is for an installed
  // app.
  if (app_id_.empty())
    return;

  // There is no way to reliably know if |app_id_| is for a System Web App
  // during startup, so we always call MaybeUpdateFileHandlingOriginTrialExpiry.
  provider_->os_integration_manager().MaybeUpdateFileHandlingOriginTrialExpiry(
      web_contents(), app_id_);
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

  // TODO(crbug.com/1053371): Clean up where we install file handlers.
  provider_->os_integration_manager().MaybeUpdateFileHandlingOriginTrialExpiry(
      web_contents(), installed_app_id);
}

void WebAppTabHelper::OnWebAppWillBeUninstalled(
    const AppId& uninstalled_app_id) {
  if (GetAppId() == uninstalled_app_id)
    ResetAppId();
}

void WebAppTabHelper::OnAppRegistrarShutdown() {
  ResetAppId();
}

void WebAppTabHelper::OnAppRegistrarDestroyed() {
  observer_.RemoveAll();
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

}  // namespace web_app
