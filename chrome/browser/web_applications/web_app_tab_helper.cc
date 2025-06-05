// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_tab_helper.h"

#include <memory>
#include <optional>
#include <string>

#include "base/check_is_test.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_launch_queue_delegate_impl.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"

namespace web_app {

// static
void WebAppTabHelper::Create(tabs::TabInterface* tab,
                             content::WebContents* contents) {
  // In the event when a tab is moved from a normal browser window to an app
  // window, or vise versa, we want to keep the state on WebAppTabHelper.
  auto* tab_helper = WebAppTabHelper::FromWebContents(contents);
  if (tab->GetContents() == contents && tab_helper) {
    tab_helper->SubscribeToTabState(tab);
    return;
  }

  // If on the other hand this is a tab-discard, we let the old tab's
  // WebAppTabHelper be destroyed at its normal timing. This is because the
  // current implementation of WebAppMetrics relies on the assumption that
  // discarded WebContents are still usable.
  // This will become a moot point once https://crbug.com/347770670 is fixed, as
  // discarding will no longer change the WebContents.

  auto helper = std::make_unique<WebAppTabHelper>(tab, contents);
  helper->SubscribeToTabState(tab);
  contents->SetUserData(UserDataKey(), std::move(helper));
}

// static
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
      web_app_provider->registrar_unsafe().GetInstallState(*app_id) !=
          proto::INSTALLED_WITH_OS_INTEGRATION) {
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

webapps::LaunchQueue& WebAppTabHelper::EnsureLaunchQueue() {
  if (!launch_queue_) {
    std::unique_ptr<webapps::LaunchQueueDelegate> delegate =
        std::make_unique<LaunchQueueDelegateImpl>(
            provider_->registrar_unsafe());
    launch_queue_ = std::make_unique<webapps::LaunchQueue>(web_contents(),
                                                           std::move(delegate));
  }
  return *launch_queue_;
}

void WebAppTabHelper::SetState(std::optional<webapps::AppId> app_id,
                               std::optional<webapps::AppId> window_app_id) {
  // Empty string should not be used to indicate "no app ID".
  DCHECK(!app_id || !app_id->empty());

  // If the app_id is changing, then it should exist in the database.
  DCHECK(app_id_ == app_id || !app_id ||
         provider_->registrar_unsafe().IsInstallState(
             *app_id, {proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
                       proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                       proto::InstallState::INSTALLED_WITH_OS_INTEGRATION}) ||
         provider_->registrar_unsafe().IsUninstalling(*app_id));

  if (app_id_ == app_id && window_app_id_ == window_app_id) {
    // This can be triggered for navigations that are happening in the same app
    // window, like if a navigation is captured in an open window causing a page
    // load to happen. Record the `UseCounter` there as well, as that is
    // treated as an app launch.
    ScheduleManifestAppliedUseCounter();
    return;
  }

  std::optional<webapps::AppId> previous_app_id = std::move(app_id_);
  app_id_ = std::move(app_id);
  window_app_id_ = std::move(window_app_id);

  if (previous_app_id != app_id_) {
    OnAssociatedAppChanged(previous_app_id, app_id_);
  }
  UpdateAudioFocusGroupId();
  ScheduleManifestAppliedUseCounter();
}

void WebAppTabHelper::SetAppId(std::optional<webapps::AppId> app_id) {
  SetState(std::move(app_id), window_app_id_);
}

void WebAppTabHelper::SetIsInAppWindow(
    std::optional<webapps::AppId> window_app_id) {
  SetState(app_id(), std::move(window_app_id));
}

void WebAppTabHelper::SetCallbackToRunOnTabChanges(base::OnceClosure callback) {
  on_tab_details_changed_callback_ = std::move(callback);
}

void WebAppTabHelper::OnTabBackgrounded(tabs::TabInterface*) {
  MaybeNotifyTabChanged();
}

void WebAppTabHelper::OnTabDetached(tabs::TabInterface* tab_interface,
                                    tabs::TabInterface::DetachReason) {
  MaybeNotifyTabChanged();
}

void WebAppTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    const GURL& url = navigation_handle->GetURL();
    SetAppId(provider_->registrar_unsafe().FindBestAppWithUrlInScope(
        url, web_app::WebAppFilter::InstalledInChrome()));
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

void WebAppTabHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& validated_url) {
  can_record_manifest_applied_ = true;
  MaybeRecordManifestAppliedUseCounter();
}

void WebAppTabHelper::FlushLaunchQueueForTesting() const {
  if (!launch_queue_) {
    return;
  }
  launch_queue_->FlushForTesting();  // IN-TEST
}

WebAppTabHelper::WebAppTabHelper(tabs::TabInterface* tab,
                                 content::WebContents* contents)
    : content::WebContentsUserData<WebAppTabHelper>(*contents),
      content::WebContentsObserver(contents) {
  CHECK(AreWebAppsEnabled(tab->GetBrowserWindowInterface()->GetProfile()));
  provider_ = WebAppProvider::GetForLocalAppsUnchecked(
      tab->GetBrowserWindowInterface()->GetProfile());
  CHECK(provider_);
  observation_.Observe(&provider_->install_manager());
  SetState(provider_->registrar_unsafe().FindBestAppWithUrlInScope(
               contents->GetLastCommittedURL(),
               web_app::WebAppFilter::InstalledInChrome()),
           /*window_app_id=*/std::nullopt);
}

bool WebAppTabHelper::CanBeUsedForFocusExisting() const {
  constexpr std::array<std::string_view, 3>
      kMimeTypesWithExpectedLaunchConsumer = {
          "text/html",
          "text/xhtml+xml",
          "application/xhtml+xml",
      };

  const std::string& mime_type = web_contents()->GetContentsMimeType();
  for (std::string_view allowed_mime_type :
       kMimeTypesWithExpectedLaunchConsumer) {
    if (mime_type == allowed_mime_type) {
      return true;
    }
  }

  const network::mojom::URLResponseHead* response_head =
      web_contents()->GetPrimaryMainFrame()->GetLastResponseHead();
  if (response_head) {
    for (std::string_view allowed_mime_type :
         kMimeTypesWithExpectedLaunchConsumer) {
      if (response_head->mime_type == allowed_mime_type) {
        return true;
      }
    }
  }

  return false;
}

void WebAppTabHelper::OnWebAppInstalled(
    const webapps::AppId& installed_app_id) {
  // Check if current web_contents url is in scope for the newly installed app.
  std::optional<webapps::AppId> app_id =
      provider_->registrar_unsafe().FindBestAppWithUrlInScope(
          web_contents()->GetLastCommittedURL(),
          web_app::WebAppFilter::InstalledInChrome());
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

void WebAppTabHelper::OnAssociatedAppChanged(
    const std::optional<webapps::AppId>& previous_app_id,
    const std::optional<webapps::AppId>& new_app_id) {
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
  // TODO(https://crbug.com/378970240): Perhaps check that these values are
  // equal.
  if (app_id_.has_value() && window_app_id_.has_value()) {
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

void WebAppTabHelper::SubscribeToTabState(tabs::TabInterface* tab_interface) {
  tab_subscriptions_.clear();
  CHECK(tab_interface);
  tab_subscriptions_.push_back(
      tab_interface->RegisterWillDeactivate(base::BindRepeating(
          &WebAppTabHelper::OnTabBackgrounded, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(
      tab_interface->RegisterWillDetach(base::BindRepeating(
          &WebAppTabHelper::OnTabDetached, weak_factory_.GetWeakPtr())));
}

void WebAppTabHelper::MaybeNotifyTabChanged() {
  if (on_tab_details_changed_callback_) {
    std::move(on_tab_details_changed_callback_).Run();
  }
}

void WebAppTabHelper::ScheduleManifestAppliedUseCounter() {
  bool should_measure_use_counter_for_standalone_launch =
      app_id_.has_value() && app_id_ == window_app_id_ &&
      !provider_->registrar_unsafe().GetAppManifestUrl(*app_id_).is_empty();
  if (!should_measure_use_counter_for_standalone_launch) {
    return;
  }
  meaure_manifest_applied_use_counter_ = true;
  MaybeRecordManifestAppliedUseCounter();
}

void WebAppTabHelper::MaybeRecordManifestAppliedUseCounter() {
  if (!meaure_manifest_applied_use_counter_ || !can_record_manifest_applied_) {
    return;
  }

  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      web_contents()->GetPrimaryMainFrame(),
      blink::mojom::WebFeature::kInstalledManifestApplied);
  meaure_manifest_applied_use_counter_ = false;
  can_record_manifest_applied_ = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelper);

}  // namespace web_app
