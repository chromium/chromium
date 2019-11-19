// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_tab_helper.h"

#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/manifest_update_manager.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"

namespace web_app {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelper)

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

void WebAppTabHelper::SetAppId(const AppId& app_id) {
  DCHECK(app_id.empty() || provider_->registrar().IsInstalled(app_id));
  if (app_id_ == app_id)
    return;

  app_id_ = app_id;

  OnAssociatedAppChanged();
}

void WebAppTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  const GURL& url = navigation_handle->GetURL();
  const AppId app_id = FindAppIdWithUrlInScope(url);
  SetAppId(app_id);

  provider_->manifest_update_manager().MaybeUpdate(url, app_id, web_contents());

  ReinstallPlaceholderAppIfNecessary(navigation_handle->GetURL());
}

void WebAppTabHelper::DidCloneToNewWebContents(
    content::WebContents* old_web_contents,
    content::WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a WebAppTabHelper.
  CreateForWebContents(new_web_contents);
  auto* new_tab_helper = FromWebContents(new_web_contents);

  // Clone common state:
  new_tab_helper->SetAppId(app_id());
}

bool WebAppTabHelper::IsUserInstalled() const {
  return !app_id_.empty() && provider_->registrar().WasInstalledByUser(app_id_);
}

bool WebAppTabHelper::IsFromInstallButton() const {
  // TODO(loyso): Use something better to record apps installed from promoted
  // UIs. crbug.com/774918.
  return !app_id_.empty() &&
         provider_->registrar().GetAppScope(app_id_).has_value();
}

bool WebAppTabHelper::IsInAppWindow() const {
  return provider_->ui_manager().IsInAppWindow(web_contents());
}

void WebAppTabHelper::OnWebAppInstalled(const AppId& installed_app_id) {
  // Check if current web_contents url is in scope for the newly installed app.
  AppId app_id = FindAppIdWithUrlInScope(web_contents()->GetURL());
  if (app_id == installed_app_id)
    SetAppId(app_id);
}

void WebAppTabHelper::OnWebAppUninstalled(const AppId& uninstalled_app_id) {
  if (app_id() == uninstalled_app_id)
    ResetAppId();
}

void WebAppTabHelper::OnAppRegistrarShutdown() {
  ResetAppId();
}

void WebAppTabHelper::OnAppRegistrarDestroyed() {
  observer_.RemoveAll();
}

void WebAppTabHelper::ResetAppId() {
  app_id_.clear();

  OnAssociatedAppChanged();
}

void WebAppTabHelper::OnAssociatedAppChanged() {
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
