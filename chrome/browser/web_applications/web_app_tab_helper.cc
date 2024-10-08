// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_tab_helper.h"

#include <memory>
#include <string>

#include "base/check_is_test.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/web_app_launch_queue.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_ui_state_manager.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace web_app {

const webapps::AppId* WebAppTabHelper::GetAppId(
    const content::WebContents* web_contents) {
  auto* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->app_id_.has_value()
             ? &tab_helper->app_id_.value()
             : nullptr;
}

#if BUILDFLAG(IS_MAC)
std::optional<webapps::AppId>
WebAppTabHelper::GetAppIdForNotificationAttribution(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          features::kAppShimNotificationAttribution)) {
    return std::nullopt;
  }
  const webapps::AppId* app_id = GetAppId(web_contents);
  if (!app_id) {
    return std::nullopt;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  WebAppProvider* web_app_provider = WebAppProvider::GetForWebApps(profile);
  if (!web_app_provider ||
      !web_app_provider->registrar_unsafe().IsInstallState(
          *app_id, {proto::INSTALLED_WITH_OS_INTEGRATION})) {
    return std::nullopt;
  }
  // Default apps are locally installed but unless an app shim has been created
  // for them should not get attributed notifications.
  if (!AppShimRegistry::Get()->IsAppInstalledInProfile(*app_id,
                                                       profile->GetPath())) {
    return std::nullopt;
  }
  return *app_id;
}
#endif

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

void WebAppTabHelper::SetState(std::optional<webapps::AppId> app_id,
                               bool is_in_app_window) {
  // Empty string should not be used to indicate "no app ID".
  DCHECK(!app_id || !app_id->empty());

  // If the app_id is changing, then it should exist in the database.
  DCHECK(app_id_ == app_id || !app_id ||
         provider_->registrar_unsafe().IsInstalled(*app_id) ||
         provider_->registrar_unsafe().IsUninstalling(*app_id));
  if (app_id_ == app_id && is_in_app_window == is_in_app_window_) {
    return;
  }

  std::optional<webapps::AppId> previous_app_id = std::move(app_id_);
  app_id_ = std::move(app_id);

  is_in_app_window_ = is_in_app_window;

  if (previous_app_id != app_id_) {
    OnAssociatedAppChanged(previous_app_id, app_id_);
  }
  UpdateAudioFocusGroupId();
}

void WebAppTabHelper::SetAppId(std::optional<webapps::AppId> app_id) {
  SetState(app_id, is_in_app_window());
}

void WebAppTabHelper::SetIsInAppWindow(bool is_in_app_window) {
  SetState(app_id(), is_in_app_window);
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
  new_tab_helper->SetState(app_id_, /*is_in_app_window=*/false);
  // Note: We don't clone is_in_app_window, as that need to only be set when
  // the new web contents is added to an app window.
}

WebAppTabHelper::WebAppTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<WebAppTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  observation_.Observe(&provider_->install_manager());
  SetState(FindAppWithUrlInScope(web_contents->GetLastCommittedURL()),
           /*is_in_app_window=*/false);
}

void WebAppTabHelper::OnWebAppInstalled(
    const webapps::AppId& installed_app_id) {
  // Check if current web_contents url is in scope for the newly installed app.
  std::optional<webapps::AppId> app_id =
      FindAppWithUrlInScope(web_contents()->GetLastCommittedURL());
  if (app_id == installed_app_id) {
    SetAppId(app_id);
  }
}

void WebAppTabHelper::OnWebAppWillBeUninstalled(
    const webapps::AppId& uninstalled_app_id) {
  if (app_id_ == uninstalled_app_id) {
    SetAppId(std::nullopt);
  }
}

void WebAppTabHelper::OnWebAppInstallManagerDestroyed() {
  observation_.Reset();
  SetAppId(std::nullopt);
}

void WebAppTabHelper::InitForTabFeatures(tabs::TabInterface* tab) {
  tab_subscriptions_.push_back(tab->RegisterDidEnterForeground(
      base::BindRepeating(&WebAppTabHelper::TabDidEnterForeground,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab->RegisterWillEnterBackground(
      base::BindRepeating(&WebAppTabHelper::TabWillEnterBackground,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab->RegisterWillDetach(base::BindRepeating(
      &WebAppTabHelper::WillDetach, weak_factory_.GetWeakPtr())));
}

void WebAppTabHelper::TabDidEnterForeground(tabs::TabInterface* tab) {
  if (app_id_.has_value()) {
    provider_->ui_state_manager().NotifyWebAppWindowDidEnterForeground(
        app_id_.value());
  }
}

void WebAppTabHelper::TabWillEnterBackground(tabs::TabInterface* tab) {
  if (app_id_.has_value()) {
    provider_->ui_state_manager().NotifyWebAppWindowWillEnterBackground(
        app_id_.value());
  }
}

void WebAppTabHelper::WillDetach(tabs::TabInterface* tab,
                                 tabs::TabInterface::DetachReason reason) {
  switch (reason) {
    case tabs::TabInterface::DetachReason::kDelete:
      tab_subscriptions_.clear();
      break;
    case tabs::TabInterface::DetachReason::kInsertIntoOtherWindow:
      break;
  }
}

void WebAppTabHelper::OnAssociatedAppChanged(
    const std::optional<webapps::AppId>& previous_app_id,
    const std::optional<webapps::AppId>& new_app_id) {
  provider_->ui_manager().NotifyOnAssociatedAppChanged(
      web_contents(), previous_app_id, new_app_id);

  // Tag WebContents for Task Manager.
  // cases to consider:
  // 1. non-app -> app (association added)
  // 2. non-app -> non-app
  // 3. app -> app (association changed)
  // 4. app -> non-app (association removed)

  if (new_app_id.has_value() && !new_app_id->empty()) {
    // case 1 & 3:
    // WebContents could already be tagged with TabContentsTag or WebAppTag,
    // therefore we want to clear it.
    task_manager::WebContentsTags::ClearTag(web_contents());
    task_manager::WebContentsTags::CreateForWebApp(
        web_contents(), new_app_id.value(),
        provider_->registrar_unsafe().IsIsolated(new_app_id.value()));
  } else {
    // case 4:
    if (previous_app_id.has_value() && !previous_app_id->empty()) {
      // remove WebAppTag, add TabContentsTag.
      task_manager::WebContentsTags::ClearTag(web_contents());
      task_manager::WebContentsTags::CreateForTabContents(web_contents());
    }
    // case 2: do nothing
  }
}

void WebAppTabHelper::UpdateAudioFocusGroupId() {
  if (app_id_.has_value() && is_in_app_window_) {
    audio_focus_group_id_ =
        provider_->audio_focus_id_map().CreateOrGetIdForApp(app_id_.value());
  } else {
    audio_focus_group_id_ = base::UnguessableToken::Null();
  }

  // There is no need to trigger creation of a MediaSession if we'd merely be
  // resetting the audo focus group id as that is the default state. Skipping
  // creating a MediaSession when not needed also helps with some (unit) tests
  // where creating a MediaSession can trigger other subsystems in ways that
  // the test might not be setup for (for example lack of a real io thread for
  // the mdns service).
  if (audio_focus_group_id_ == base::UnguessableToken::Null() &&
      !content::MediaSession::GetIfExists(web_contents())) {
    return;
  }
  content::MediaSession::Get(web_contents())
      ->SetAudioFocusGroupId(audio_focus_group_id_);
}

void WebAppTabHelper::ReinstallPlaceholderAppIfNecessary(const GURL& url) {
  provider_->policy_manager().ReinstallPlaceholderAppIfNecessary(
      url, base::DoNothing());
}

std::optional<webapps::AppId> WebAppTabHelper::FindAppWithUrlInScope(
    const GURL& url) const {
  return provider_->registrar_unsafe().FindAppWithUrlInScope(url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelper);

}  // namespace web_app
