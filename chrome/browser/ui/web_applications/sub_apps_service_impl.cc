// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif

using blink::mojom::SubAppsService;
using blink::mojom::SubAppsServiceAddParametersPtr;
using blink::mojom::SubAppsServiceAddResult;
using blink::mojom::SubAppsServiceAddResultPtr;
using blink::mojom::SubAppsServiceListResult;
using blink::mojom::SubAppsServiceListResultEntry;
using blink::mojom::SubAppsServiceListResultEntryPtr;
using blink::mojom::SubAppsServiceRemoveResult;
using blink::mojom::SubAppsServiceRemoveResultPtr;
using blink::mojom::SubAppsServiceResultCode;

namespace web_app {

// TODO(crbug.com/40924576): Replace registrar_unsafe with Locks
// TODO (crbug.com/1467862): Move from //c/b/ui/web_applications to
// //c/b/web_applications

namespace {

constexpr char kSubAppsUninstallNotifierId[] = "sub_apps_service";

// Resolve string `path` with `origin`, and if the resulting GURL isn't same
// origin with `origin` then return an error (for which the caller needs to
// raise a `ReportBadMessageAndDeleteThis`).
base::expected<GURL, std::string> ConvertPathToUrl(const std::string& path,
                                                   const url::Origin& origin) {
  GURL resolved = origin.GetURL().Resolve(path);

  if (!origin.IsSameOriginWith(resolved)) {
    return base::unexpected(
        "SubAppsServiceImpl: Different origin arg to that of the calling app.");
  }

  if (resolved.is_empty()) {
    return base::unexpected("SubAppsServiceImpl: Empty url.");
  }

  if (!resolved.is_valid()) {
    return base::unexpected("SubAppsServiceImpl: Invalid url.");
  }

  return base::ok(resolved);
}

std::string ConvertUrlToPath(const webapps::ManifestId& manifest_id) {
  return manifest_id.PathForRequest();
}

base::expected<std::vector<SubAppInstallParams>, std::string>
AddOptionsFromMojo(
    const url::Origin& origin,
    const std::vector<SubAppsServiceAddParametersPtr>& sub_apps_to_add_mojo) {
  std::vector<SubAppInstallParams> sub_apps;
  for (const auto& sub_app : sub_apps_to_add_mojo) {
    ASSIGN_OR_RETURN(webapps::ManifestId manifest_id,
                     ConvertPathToUrl(sub_app->manifest_id_path, origin));
    ASSIGN_OR_RETURN(GURL install_url,
                     ConvertPathToUrl(sub_app->install_url_path, origin));
    sub_apps.emplace_back(std::move(manifest_id), std::move(install_url));
  }
  return sub_apps;
}

Profile* GetProfile(content::RenderFrameHost& render_frame_host) {
  return Profile::FromBrowserContext(render_frame_host.GetBrowserContext());
}

WebAppProvider* GetWebAppProvider(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  auto* provider = WebAppProvider::GetForWebContents(initiator_web_contents);
  DCHECK(provider);
  return provider;
}

const webapps::AppId* GetAppId(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  return WebAppTabHelper::GetAppId(initiator_web_contents);
}

blink::mojom::SubAppsServiceResultCode InstallResultCodeToMojo(
    webapps::InstallResultCode install_result_code) {
  return webapps::IsSuccess(install_result_code)
             ? blink::mojom::SubAppsServiceResultCode::kSuccess
             : blink::mojom::SubAppsServiceResultCode::kFailure;
}

void ReturnAllAddsAsFailed(
    const std::vector<SubAppsServiceAddParametersPtr>& sub_apps,
    SubAppsServiceImpl::AddCallback result_callback) {
  std::vector<SubAppsServiceAddResultPtr> result;
  for (const auto& sub_app : sub_apps) {
    result.emplace_back(SubAppsServiceAddResult::New(
        sub_app->manifest_id_path, SubAppsServiceResultCode::kFailure));
  }
  std::move(result_callback).Run(std::move(result));
}

bool IsInstalledNonChildApp(content::RenderFrameHost& render_frame_host) {
  auto* app_id = GetAppId(render_frame_host);
  if (!app_id) {
    return false;
  }

  auto* provider = GetWebAppProvider(render_frame_host);
  auto* web_app = provider->registrar_unsafe().GetAppById(*app_id);
  return (web_app && !web_app->IsSubAppInstalledApp());
}

// Verify that the calling app has the SubApps permissions policy set and that
// it is an installed IWA that is not a sub app itself. This check is called
// from `CreateIfAllowed` and from each of the APIs entry points to avoid a
// potential race between the parent app calling an API while being uninstalled.
bool CanAccessSubAppsApi(content::RenderFrameHost& render_frame_host) {
  return render_frame_host.IsFeatureEnabled(
             blink::mojom::PermissionsPolicyFeature::kSubApps) &&
         content::HasIsolatedContextCapability(&render_frame_host) &&
         IsInstalledNonChildApp(render_frame_host);
}

bool ShouldSkipUserConfirmation(content::RenderFrameHost& frame) {
#if BUILDFLAG(IS_CHROMEOS)
  auto const* profile = Profile::FromBrowserContext(frame.GetBrowserContext());
  if (!profile) {
    return false;
  }

  auto const* prefs = profile->GetPrefs();
  if (!prefs) {
    return false;
  }

  return policy::IsOriginInAllowlist(
      frame.GetLastCommittedURL(), prefs,
      prefs::kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins);
#else   // BUILDFLAG(IS_CHROMEOS)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

SubAppsServiceImpl::SubAppsServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<SubAppsService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

SubAppsServiceImpl::~SubAppsServiceImpl() = default;

SubAppsServiceImpl::AddCallInfo::AddCallInfo() = default;
SubAppsServiceImpl::AddCallInfo::~AddCallInfo() = default;

// static
void SubAppsServiceImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<SubAppsService> receiver) {
  CHECK(render_frame_host);

  // This class is created only on the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    receiver.reset();
    return;
  }

  // Bail if Web Apps aren't enabled on current profile.
  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    receiver.reset();
    return;
  }

  // Bail if the calling app is not an Isolated Web App or is not installed or
  // is a sub-app itself.
  if (!CanAccessSubAppsApi(*render_frame_host)) {
    receiver.reset();
    return;
  }

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new SubAppsServiceImpl(*render_frame_host, std::move(receiver));
}

void SubAppsServiceImpl::Add(
    std::vector<SubAppsServiceAddParametersPtr> sub_apps_to_add,
    AddCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&SubAppsServiceImpl::Add, weak_ptr_factory_.GetWeakPtr(),
                       std::move(sub_apps_to_add), std::move(result_callback)));
    return;
  }

  if (!CanAccessSubAppsApi(render_frame_host())) {
    ReturnAllAddsAsFailed(sub_apps_to_add, std::move(result_callback));
    return;
  }

  if (sub_apps_to_add.empty()) {
    std::move(result_callback).Run({});
    return;
  }

  // Check if origin is embargoed because of too many dismissals.
  if (PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host().GetBrowserContext()))
          ->IsEmbargoed(render_frame_host().GetLastCommittedOrigin().GetURL(),
                        ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS)) {
    ReturnAllAddsAsFailed(sub_apps_to_add, std::move(result_callback));
    return;
  }

  ASSIGN_OR_RETURN(
      (std::vector<SubAppInstallParams> add_options),
      AddOptionsFromMojo(render_frame_host().GetLastCommittedOrigin(),
                         sub_apps_to_add),
      // Compromised renderer, bail immediately (this call deletes *this).
      &SubAppsServiceImpl::ReportBadMessageAndDeleteThis, this);

  CHECK(AreWebAppsUserInstallable(
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext())));

  // Assign id to this add call
  int add_call_id = next_add_call_id_++;
  AddCallInfo& add_call_info = add_call_info_[add_call_id];
  add_call_info.mojo_callback = std::move(result_callback);

  auto parent_manifest_id = provider->registrar_unsafe()
                                .GetAppById(*GetAppId(render_frame_host()))
                                ->manifest_id();
  CollectInstallData(add_call_id, std::move(add_options), parent_manifest_id);
}

void SubAppsServiceImpl::CollectInstallData(
    int add_call_id,
    std::vector<SubAppInstallParams> requested_installs,
    webapps::ManifestId parent_manifest_id) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  base::ConcurrentCallbacks<
      std::pair<webapps::ManifestId, std::unique_ptr<WebAppInstallInfo>>>
      concurrent;

  // Schedule data collection for each requested install
  for (const auto& [manifest_id, url_to_load] : requested_installs) {
    // Check if app is the parent app itself
    if (manifest_id == parent_manifest_id) {
      add_call_info_.at(add_call_id)
          .results.emplace_back(SubAppsServiceAddResult::New(
              ConvertUrlToPath(manifest_id),
              blink::mojom::SubAppsServiceResultCode::kFailure));
      continue;
    }

    // Check if app is already installed as a sub app
    if (provider->registrar_unsafe().WasInstalledBySubApp(
            GenerateAppIdFromManifestId(manifest_id, parent_manifest_id))) {
      add_call_info_.at(add_call_id)
          .results.emplace_back(SubAppsServiceAddResult::New(
              ConvertUrlToPath(manifest_id),
              blink::mojom::SubAppsServiceResultCode::kSuccess));
      continue;
    }

    provider->scheduler().FetchInstallInfoFromInstallUrl(
        manifest_id, url_to_load, parent_manifest_id,
        base::BindOnce(
            [](webapps::ManifestId manifest_app_id,
               std::unique_ptr<WebAppInstallInfo> install_info) {
              return std::pair(manifest_app_id, std::move(install_info));
            },
            manifest_id)
            .Then(concurrent.CreateCallback()));
  }

  std::move(concurrent)
      .Done(base::BindOnce(&SubAppsServiceImpl::ProcessInstallData,
                           weak_ptr_factory_.GetWeakPtr(), add_call_id));
}

void SubAppsServiceImpl::ProcessInstallData(
    int add_call_id,
    std::vector<std::pair<webapps::ManifestId,
                          std::unique_ptr<WebAppInstallInfo>>> install_data) {
  AddCallInfo& add_call_info = add_call_info_.at(add_call_id);
  const webapps::AppId* parent_app_id = GetAppId(render_frame_host());

  for (auto& [manifest_id, install_info] : install_data) {
    // If manifest_id is empty, the app was already installed and no install
    // info was collected. If it is invalid, do not to trigger an installation
    // since the command will run into problems.
    if (!manifest_id.is_valid()) {
      continue;
    }

    if (install_info) {
      install_info->parent_app_id = *parent_app_id;
      install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
      add_call_info.install_infos.emplace_back(std::move(install_info));
    } else {
      // Log error if install info could not be loaded
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(manifest_id),
          blink::mojom::SubAppsServiceResultCode::kFailure));
    }
  }

  FinishAddCallOrShowInstallDialog(add_call_id);
}

void SubAppsServiceImpl::FinishAddCallOrShowInstallDialog(int add_call_id) {
  AddCallInfo& add_call_info = add_call_info_.at(add_call_id);

  if (add_call_info.install_infos.empty()) {
    FinishAddCall(add_call_id, {});
    return;
  }

  if (ShouldSkipUserConfirmation(render_frame_host())) {
    ProcessDialogResponse(add_call_id, true);
    return;
  }

  WebAppRegistrar& registrar =
      GetWebAppProvider(render_frame_host())->registrar_unsafe();
  const webapps::AppId* parent_app_id = GetAppId(render_frame_host());

  add_call_info.install_dialog =
      std::make_unique<SubAppsInstallDialogController>();
  add_call_info.install_dialog->Init(
      base::BindOnce(&SubAppsServiceImpl::ProcessDialogResponse,
                     weak_ptr_factory_.GetWeakPtr(), add_call_id),
      add_call_info.install_infos,
      /*parent_app_name=*/registrar.GetAppShortName(*parent_app_id),
      *parent_app_id, GetProfile(render_frame_host()),
      /*window=*/
      content::WebContents::FromRenderFrameHost(&render_frame_host())
          ->GetTopLevelNativeWindow());
}

void SubAppsServiceImpl::ProcessDialogResponse(int add_call_id,
                                               bool dialog_accepted) {
  if (dialog_accepted) {
    PermissionDecisionAutoBlockerFactory::GetForProfile(
        Profile::FromBrowserContext(render_frame_host().GetBrowserContext()))
        ->RemoveEmbargoAndResetCounts(
            render_frame_host().GetLastCommittedOrigin().GetURL(),
            ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS);

    ScheduleSubAppInstalls(add_call_id);
    return;
  }

  // Dialog was declined.
  PermissionDecisionAutoBlockerFactory::GetForProfile(
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext()))
      ->RecordDismissAndEmbargo(
          render_frame_host().GetLastCommittedOrigin().GetURL(),
          ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS,
          /*dismissed_prompt_was_quiet=*/false);

  AddCallInfo& add_call_info = add_call_info_.at(add_call_id);

  for (const std::unique_ptr<web_app::WebAppInstallInfo>& install_info :
       add_call_info.install_infos) {
    add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
        ConvertUrlToPath(install_info->manifest_id()),
        blink::mojom::SubAppsServiceResultCode::kFailure));
  }

  FinishAddCall(add_call_id, {});
}

void SubAppsServiceImpl::ScheduleSubAppInstalls(int add_call_id) {
  AddCallInfo& add_call_info = add_call_info_.at(add_call_id);

  // Schedule install for each install_info that was collected
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  base::ConcurrentCallbacks<SubAppInstallResult> concurrent;
  for (auto& install_info : add_call_info.install_infos) {
    webapps::ManifestId manifest_id = install_info->manifest_id();
    provider->scheduler().InstallFromInfoWithParams(
        std::move(install_info), /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::SUB_APP,
        base::BindOnce(
            [](webapps::ManifestId manifest_id, const webapps::AppId& app_id,
               webapps::InstallResultCode result_code) {
              return SubAppInstallResult(manifest_id, app_id, result_code);
            },
            manifest_id)
            .Then(concurrent.CreateCallback()),
        WebAppInstallParams());
  }
  std::move(concurrent)
      .Done(base::BindOnce(&SubAppsServiceImpl::FinishAddCall,
                           weak_ptr_factory_.GetWeakPtr(), add_call_id));
}

void SubAppsServiceImpl::FinishAddCall(
    int add_call_id,
    std::vector<SubAppInstallResult> install_results) {
  AddCallInfo& add_call_info = add_call_info_.at(add_call_id);

  for (const auto& [manifest_id, app_id, result_code] : install_results) {
    add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
        ConvertUrlToPath(manifest_id), InstallResultCodeToMojo(result_code)));
  }

  std::move(add_call_info.mojo_callback).Run(std::move(add_call_info.results));

  add_call_info_.erase(add_call_id);
}

void SubAppsServiceImpl::List(ListCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE, base::BindOnce(&SubAppsServiceImpl::List,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(result_callback)));
    return;
  }

  if (!CanAccessSubAppsApi(render_frame_host())) {
    return std::move(result_callback)
        .Run(SubAppsServiceListResult::New(
            SubAppsServiceResultCode::kFailure,
            std::vector<SubAppsServiceListResultEntryPtr>()));
  }

  const WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::vector<SubAppsServiceListResultEntryPtr> sub_apps_list;
  for (const webapps::AppId& sub_app_id :
       registrar.GetAllSubAppIds(*GetAppId(render_frame_host()))) {
    webapps::ManifestId manifest_id = registrar.GetAppManifestId(sub_app_id);
    CHECK(manifest_id.is_valid());
    sub_apps_list.push_back(SubAppsServiceListResultEntry::New(
        ConvertUrlToPath(manifest_id), registrar.GetAppShortName(sub_app_id)));
  }

  std::move(result_callback)
      .Run(SubAppsServiceListResult::New(SubAppsServiceResultCode::kSuccess,
                                         std::move(sub_apps_list)));
}

void SubAppsServiceImpl::Remove(
    const std::vector<std::string>& manifest_id_paths,
    RemoveCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&SubAppsServiceImpl::Remove,
                       weak_ptr_factory_.GetWeakPtr(), manifest_id_paths,
                       std::move(result_callback)));
    return;
  }

  if (!CanAccessSubAppsApi(render_frame_host())) {
    std::vector<SubAppsServiceRemoveResultPtr> result;
    for (const std::string& manifest_id_path : manifest_id_paths) {
      result.emplace_back(SubAppsServiceRemoveResult::New(
          manifest_id_path, SubAppsServiceResultCode::kFailure));
    }

    return std::move(result_callback).Run(std::move(result));
  }

  // Take weak pointer early as this may get deleted by RemoveSubApp().
  base::WeakPtr<SubAppsServiceImpl> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  base::ConcurrentCallbacks<SubAppsServiceRemoveResultPtr> concurrent;
  for (const std::string& manifest_id_path : manifest_id_paths) {
    RemoveSubApp(manifest_id_path, concurrent.CreateCallback(),
                 GetAppId(render_frame_host()));
  }
  std::move(concurrent)
      .Done(base::BindOnce(&SubAppsServiceImpl::NotifyUninstall, weak_ptr,
                           std::move(result_callback)));
}

void SubAppsServiceImpl::RemoveSubApp(
    const std::string& manifest_id_path,
    base::OnceCallback<void(SubAppsServiceRemoveResultPtr)> callback,
    const webapps::AppId* calling_app_id) {
  // Convert `manifest_id_path` from path form to full URL form.
  ASSIGN_OR_RETURN(
      const webapps::ManifestId manifest_id,
      ConvertPathToUrl(manifest_id_path,
                       render_frame_host().GetLastCommittedOrigin()),
      // Compromised renderer, bail immediately (this call deletes *this).
      &SubAppsServiceImpl::ReportBadMessageAndDeleteThis, this);

  WebAppProvider* provider = GetWebAppProvider(render_frame_host());

  const webapps::AppId* parent_app_id = GetAppId(render_frame_host());
  if (!parent_app_id) {
    return ReportBadMessageAndDeleteThis("Parent app id is null");
  }

  webapps::ManifestId parent_manifest_id =
      provider->registrar_unsafe().GetAppManifestId(*parent_app_id);
  if (!parent_manifest_id.is_valid()) {
    return ReportBadMessageAndDeleteThis("Parent manifest is null");
  }

  webapps::AppId sub_app_id =
      GenerateAppIdFromManifestId(manifest_id, parent_manifest_id);
  const WebApp* app = provider->registrar_unsafe().GetAppById(sub_app_id);

  // Verify that the app we're trying to remove exists, is installed and that
  // its parent_app is the one doing the current call.
  if (!app || !app->parent_app_id() ||
      *calling_app_id != *app->parent_app_id() ||
      !provider->registrar_unsafe().IsInstalled(sub_app_id)) {
    return std::move(callback).Run(SubAppsServiceRemoveResult::New(
        manifest_id_path, SubAppsServiceResultCode::kFailure));
  }

  provider->scheduler().RemoveInstallManagementMaybeUninstall(
      sub_app_id, WebAppManagement::Type::kSubApp,
      webapps::WebappUninstallSource::kSubApp,
      base::BindOnce(
          [](std::string manifest_id_path,
             webapps::UninstallResultCode result_code) {
            SubAppsServiceResultCode result =
                webapps::UninstallSucceeded(result_code)
                    ? SubAppsServiceResultCode::kSuccess
                    : SubAppsServiceResultCode::kFailure;
            return SubAppsServiceRemoveResult::New(manifest_id_path, result);
          },
          manifest_id_path)
          .Then(std::move(callback)));
}

void SubAppsServiceImpl::NotifyUninstall(
    RemoveCallback result_callback,
    std::vector<SubAppsServiceRemoveResultPtr> remove_results) {
  int num_successful_uninstalls = base::ranges::count(
      remove_results, SubAppsServiceResultCode::kSuccess,
      [](const auto& result) { return result->result_code; });

  // If any apps were uninstalled, notify the user.
  if (num_successful_uninstalls > 0) {
    WebAppRegistrar& registrar =
        GetWebAppProvider(render_frame_host())->registrar_unsafe();
    const webapps::AppId* parent_app_id = GetAppId(render_frame_host());
    const std::u16string parent_app_name =
        base::UTF8ToUTF16(registrar.GetAppShortName(*parent_app_id));
    const GURL start_url = registrar.GetAppStartUrl(*parent_app_id);
    const std::u16string title =
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_SUB_APPS_UNINSTALL_NOTIFICATION_TITLE),
            /*name0=*/"NUM_SUB_APPS", num_successful_uninstalls,
            /*name1=*/"APP_NAME", parent_app_name);
    const std::u16string message =
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_SUB_APPS_UNINSTALL_NOTIFICATION_DESCRIPTION),
            /*name0=*/"APP_NAME", parent_app_name);

    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        kSubAppsUninstallNotificationId, title, message, ui::ImageModel(),
        /*display_source=*/std::u16string(),
        /*origin_url=*/start_url,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT,
            kSubAppsUninstallNotifierId,
            ash::NotificationCatalogName::kSubAppsUninstall),
#else
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT,
            kSubAppsUninstallNotifierId),
#endif
        message_center::RichNotificationData(),
        /*delegate=*/nullptr);
    notification.SetSystemPriority();

    NotificationDisplayServiceFactory::GetForProfile(
        GetProfile(render_frame_host()))
        ->Display(NotificationHandler::Type::WEB_PERSISTENT, notification,
                  /*metadata=*/nullptr);
  }

  std::move(result_callback).Run(std::move(remove_results));
}

}  // namespace web_app
