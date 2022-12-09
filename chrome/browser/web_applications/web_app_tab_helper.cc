// Copyright 2018 The Chromium Authors
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

const AppId* WebAppTabHelper::GetAppId(content::WebContents* web_contents) {
  auto* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return nullptr;
  return tab_helper->app_id_.has_value() ? &tab_helper->app_id_.value()
                                         : nullptr;
}

WebAppTabHelper::WebAppTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<WebAppTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  DCHECK(provider_);
  observation_.Observe(&provider_->install_manager());
  SetAppId(
      FindAppWithUrlInScope(web_contents->GetSiteInstance()->GetSiteURL()));
}

WebAppTabHelper::~WebAppTabHelper() = default;

const base::UnguessableToken& WebAppTabHelper::GetAudioFocusGroupIdForTesting()
    const {
  return audio_focus_group_id_;
}

WebAppLaunchQueue& WebAppTabHelper::EnsureLaunchQueue() {
  if (!launch_queue_) {
    launch_queue_ = std::make_unique<WebAppLaunchQueue>(
        web_contents(), provider_->registrar_unsafe());
  }
  return *launch_queue_;
}

void WebAppTabHelper::SetAppId(absl::optional<AppId> app_id) {
  // Empty string should not be used to indicate "no app ID".
  DCHECK(!app_id || !app_id->empty());
  DCHECK(!app_id || provider_->registrar_unsafe().IsInstalled(*app_id) ||
         provider_->registrar_unsafe().IsUninstalling(*app_id));
  if (app_id_ == app_id)
    return;

  absl::optional<AppId> previous_app_id = std::move(app_id_);
  app_id_ = std::move(app_id);

  OnAssociatedAppChanged(previous_app_id, app_id_);
}

void WebAppTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    const GURL& url = navigation_handle->GetURL();
    SetAppId(FindAppWithUrlInScope(url));
  }

  // If navigating to a Web App (including navigation in sub frames), let
  // `WebAppUiManager` observers perform tab-secific setup for navigations in
  // Web Apps.
  if (app_id_.has_value()) {
    provider_->ui_manager().NotifyReadyToCommitNavigation(app_id_.value(),
                                                          navigation_handle);
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
      page.GetMainDocument().GetLastCommittedURL(), app_id_, web_contents());

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
  new_tab_helper->SetAppId(app_id_);
}

bool WebAppTabHelper::IsInAppWindow() const {
  return provider_->ui_manager().IsInAppWindow(web_contents());
}

void WebAppTabHelper::OnWebAppInstalled(const AppId& installed_app_id) {
  // Check if current web_contents url is in scope for the newly installed app.
  absl::optional<AppId> app_id =
      FindAppWithUrlInScope(web_contents()->GetURL());
  if (app_id == installed_app_id)
    SetAppId(app_id);
}

void WebAppTabHelper::OnWebAppWillBeUninstalled(
    const AppId& uninstalled_app_id) {
  if (app_id_ == uninstalled_app_id)
    SetAppId(absl::nullopt);
}

void WebAppTabHelper::OnWebAppInstallManagerDestroyed() {
  observation_.Reset();
  SetAppId(absl::nullopt);
}

void WebAppTabHelper::OnAssociatedAppChanged(
    const absl::optional<AppId>& previous_app_id,
    const absl::optional<AppId>& new_app_id) {
  provider_->ui_manager().NotifyOnAssociatedAppChanged(
      web_contents(), previous_app_id, new_app_id);
  UpdateAudioFocusGroupId();
}

void WebAppTabHelper::UpdateAudioFocusGroupId() {
  if (app_id_.has_value() && IsInAppWindow()) {
    audio_focus_group_id_ =
        provider_->audio_focus_id_map().CreateOrGetIdForApp(app_id_.value());
  } else {
    audio_focus_group_id_ = base::UnguessableToken::Null();
  }

  content::MediaSession::Get(web_contents())
      ->SetAudioFocusGroupId(audio_focus_group_id_);
}

void WebAppTabHelper::ReinstallPlaceholderAppIfNecessary(const GURL& url) {
  provider_->policy_manager().ReinstallPlaceholderAppIfNecessary(url);
}

absl::optional<AppId> WebAppTabHelper::FindAppWithUrlInScope(
    const GURL& url) const {
  return provider_->registrar_unsafe().FindAppWithUrlInScope(url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelper);

}  // namespace web_app
