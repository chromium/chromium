// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/sub_apps/sub_apps_service_impl.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/i18n/message_formatter.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_metrics_helper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_scope.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/notifier_catalogs.h"
#endif

using blink::mojom::SubAppsService;
using blink::mojom::SubAppsServiceAddResult;
using blink::mojom::SubAppsServiceAddResultPtr;
using blink::mojom::SubAppsServiceAddResultType;
using blink::mojom::SubAppsServiceListResultEntry;
using blink::mojom::SubAppsServiceListResultEntryPtr;
using blink::mojom::SubAppsServiceRemoveResult;
using blink::mojom::SubAppsServiceRemoveResultPtr;
using blink::mojom::SubAppsServiceRemoveResultType;
using blink::mojom::SubAppsServiceResultCode;

namespace web_app {

BASE_FEATURE(kSubAppsInstallLimit, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kSubAppsInstallLimitParam{&kSubAppsInstallLimit,
                                                        "limit", 50};

BASE_FEATURE(kSubAppsPerPromptLimit, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kSubAppsPerPromptLimitParam{
    &kSubAppsPerPromptLimit, "limit", 20};

namespace {

SubAppInstallResult::SubAppInstallResult(
    GURL install_url,
    webapps::ManifestId manifest_id,
    webapps::InstallResultCode install_result_code)
    : install_url(std::move(install_url)),
      manifest_id(std::move(manifest_id)),
      install_result_code(install_result_code) {}
SubAppInstallResult::~SubAppInstallResult() = default;
SubAppInstallResult::SubAppInstallResult(const SubAppInstallResult&) = default;
SubAppInstallResult& SubAppInstallResult::operator=(
    const SubAppInstallResult&) = default;
SubAppInstallResult::SubAppInstallResult(SubAppInstallResult&&) = default;
SubAppInstallResult& SubAppInstallResult::operator=(SubAppInstallResult&&) =
    default;

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

GURL GetUrlWithoutQueryAndRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

std::string ConvertUrlToPath(const GURL& url) {
  return url.PathForRequest();
}

std::string ConvertUrlToPath(const webapps::ManifestId& manifest_id) {
  return ConvertUrlToPath(manifest_id.value());
}

base::expected<std::vector<GURL>, std::string> AddOptionsFromMojo(
    const url::Origin& origin,
    const std::vector<std::string>& install_paths) {
  std::vector<GURL> urls;
  for (const auto& install_path : install_paths) {
    ASSIGN_OR_RETURN(GURL install_url, ConvertPathToUrl(install_path, origin));
    urls.push_back(install_url);
  }
  return urls;
}

Profile* GetProfile(content::RenderFrameHost& render_frame_host) {
  return Profile::FromBrowserContext(render_frame_host.GetBrowserContext());
}

WebAppProvider& GetWebAppProvider(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  return CHECK_DEREF(WebAppProvider::GetForWebContents(initiator_web_contents));
}

const webapps::AppId* GetAppId(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  return WebAppTabHelper::GetAppId(initiator_web_contents);
}

SubAppsServiceAddResultType InstallResultCodeToAddResultType(
    webapps::InstallResultCode install_result_code) {
  switch (install_result_code) {
    case webapps::InstallResultCode::kSuccessNewInstall:
      return SubAppsServiceAddResultType::kSuccess;
    case webapps::InstallResultCode::kSuccessAlreadyInstalled:
      return SubAppsServiceAddResultType::kAlreadyInstalled;
    default:
      return SubAppsServiceAddResultType::kGenericError;
  }
}

bool IsInstalledNonChildApp(content::RenderFrameHost& render_frame_host) {
  auto* app_id = GetAppId(render_frame_host);
  if (!app_id) {
    return false;
  }

  return GetWebAppProvider(render_frame_host)
      .registrar_unsafe()
      .AppMatches(*app_id, !WebAppFilter::IsIsolatedSubApp());
}

bool AppsScopesOverlap(
    const GURL& new_scope,
    const webapps::AppId& parent_app_id,
    const std::vector<std::unique_ptr<WebAppInstallInfo>>& collected_installs,
    WebAppRegistrar& registrar) {
  GURL parent_scope = registrar.GetAppScope(parent_app_id);
  if (IsInScope(parent_scope, new_scope)) {
    return true;
  }

  auto scopes_overlap = [&](const GURL& other_scope) {
    return IsInScope(new_scope, other_scope) ||
           IsInScope(other_scope, new_scope);
  };

  // Check against already collected sub apps in this call.
  for (const auto& existing_info : collected_installs) {
    if (scopes_overlap(existing_info->scope)) {
      return true;
    }
  }

  // Check against already installed sub apps.
  for (const webapps::AppId& installed_id :
       registrar.GetAllSubAppIds(parent_app_id)) {
    if (scopes_overlap(registrar.GetAppScope(installed_id))) {
      return true;
    }
  }

  return false;
}

ContentSetting GetSubAppsContentSetting(content::RenderFrameHost& frame) {
  auto* profile = Profile::FromBrowserContext(frame.GetBrowserContext());
  if (!profile) {
    return CONTENT_SETTING_BLOCK;
  }
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (!map) {
    return CONTENT_SETTING_BLOCK;
  }
  return map->GetContentSetting(frame.GetLastCommittedURL(),
                                frame.GetLastCommittedURL(),
                                ContentSettingsType::SUB_APPS_WITHOUT_PROMPTS);
}

IsolatedWebAppMetricsHelper::LogSubAppInstallResult
MapAddResultTypeToMetricResult(
    blink::mojom::SubAppsServiceAddResultType result_type,
    bool bypassed_prompt) {
  switch (result_type) {
    case blink::mojom::SubAppsServiceAddResultType::kSuccess:
      return bypassed_prompt
                 ? IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
                       kSuccessBypassApproval
                 : IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
                       kSuccess;
    case blink::mojom::SubAppsServiceAddResultType::kScopeOverlap:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureOverlappingScope;
    case blink::mojom::SubAppsServiceAddResultType::kRecursiveInstall:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureRecursiveInstall;
    case blink::mojom::SubAppsServiceAddResultType::kInvalidManifest:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureInvalidManifest;
    case blink::mojom::SubAppsServiceAddResultType::kAlreadyInstalled:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureAlreadyInstalled;
    case blink::mojom::SubAppsServiceAddResultType::kGenericError:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureGeneral;
  }
}

blink::mojom::SubAppsServiceResultCode MapAddCallErrorCodeToMojo(
    AddCallErrorCode result_code) {
  switch (result_code) {
    case AddCallErrorCode::kUserDeclined:
    case AddCallErrorCode::kUserDeclinedEmbargo:
      return blink::mojom::SubAppsServiceResultCode::kUserDeclined;
    case AddCallErrorCode::kTotalLimitExceeded:
      return blink::mojom::SubAppsServiceResultCode::kTotalLimitExceeded;
    case AddCallErrorCode::kPerPromptLimitExceeded:
      return blink::mojom::SubAppsServiceResultCode::kPerPromptLimitExceeded;
    case AddCallErrorCode::kWebAppsNotUserInstallable:
      return blink::mojom::SubAppsServiceResultCode::kWebAppsNotUserInstallable;
  }
}

IsolatedWebAppMetricsHelper::LogSubAppInstallResult MapAddCallErrorCodeToMetric(
    AddCallErrorCode result_code) {
  switch (result_code) {
    case AddCallErrorCode::kUserDeclined:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureUserDeclined;
    case AddCallErrorCode::kUserDeclinedEmbargo:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureUserDeclinedEmbargo;
    case AddCallErrorCode::kTotalLimitExceeded:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureNumberOfSubAppsExceedsLimit;
    case AddCallErrorCode::kPerPromptLimitExceeded:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailurePerPromptLimitExceeded;
    case AddCallErrorCode::kWebAppsNotUserInstallable:
      return IsolatedWebAppMetricsHelper::LogSubAppInstallResult::
          kFailureWebAppsNotUserInstallable;
  }
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

  if (!content::HasIsolatedContextCapability(render_frame_host)) {
    mojo::ReportBadMessage("No isolated context capability");
    return;
  }
  if (!render_frame_host->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kSubApps)) {
    mojo::ReportBadMessage("No subApps permission policy provided");
    return;
  }

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new SubAppsServiceImpl(*render_frame_host, std::move(receiver));
}

void SubAppsServiceImpl::Add(const std::vector<std::string>& install_paths,
                             AddCallback result_callback) {
  if (!IsInstalledNonChildApp(render_frame_host())) {
    std::move(result_callback)
        .Run(base::unexpected(SubAppsServiceResultCode::kWrongContext));
    return;
  }

  const WebAppRegistrar& registrar = provider().registrar_unsafe();
  auto* parent_app_id = GetAppId(render_frame_host());
  url::Origin parent_origin =
      url::Origin::Create(registrar.GetAppStartUrl(*parent_app_id));

  int add_call_id = next_add_call_id_++;
  AddCallInfo& add_call_info = add_call_info_[add_call_id];

  // Chain the callback to send metrics.
  add_call_info.mojo_callback =
      base::BindOnce(&SubAppsServiceImpl::ReportAddMetricsAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), parent_origin, add_call_id,
                     std::move(result_callback));

  if (GetSubAppsContentSetting(render_frame_host()) == CONTENT_SETTING_BLOCK) {
    std::move(add_call_info.mojo_callback)
        .Run(base::unexpected(AddCallErrorCode::kUserDeclined));
    return;
  }

  if (PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(render_frame_host().GetBrowserContext()))
          ->IsEmbargoed(render_frame_host().GetLastCommittedOrigin().GetURL(),
                        ContentSettingsType::SUB_APP_INSTALLATION_PROMPTS)) {
    std::move(add_call_info.mojo_callback)
        .Run(base::unexpected(AddCallErrorCode::kUserDeclinedEmbargo));
    return;
  }

  if (install_paths.empty()) {
    std::move(add_call_info.mojo_callback)
        .Run(std::vector<SubAppsServiceAddResultPtr>());
    return;
  }

  ASSIGN_OR_RETURN(
      (std::vector<GURL> install_urls),
      AddOptionsFromMojo(render_frame_host().GetLastCommittedOrigin(),
                         install_paths),
      // Compromised renderer, bail immediately (this call deletes *this).
      &SubAppsServiceImpl::ReportBadMessageAndDeleteThis, this);

  if (!AreWebAppsUserInstallable(Profile::FromBrowserContext(
          render_frame_host().GetBrowserContext()))) {
    std::move(add_call_info.mojo_callback)
        .Run(base::unexpected(AddCallErrorCode::kWebAppsNotUserInstallable));
    return;
  }

  // Check current limit of sub apps installed.
  // The reason is to not flood user with huge number of apps
  // to review in the UI.
  if (static_cast<int>(install_urls.size()) >
          kSubAppsPerPromptLimitParam.Get() &&
      GetSubAppsContentSetting(render_frame_host()) != CONTENT_SETTING_ALLOW) {
    std::move(add_call_info.mojo_callback)
        .Run(base::unexpected(AddCallErrorCode::kPerPromptLimitExceeded));
    return;
  }

  // Check total limit of sub apps installed.
  size_t current_count = registrar.GetAllSubAppIds(*parent_app_id).size();

  // Return all as failed if sub app limit is reached.
  // It is possible to check and install only sub apps that do not exceed the
  // limit, however, all sub app installations are rejected to simplify for
  // users reason of failure and to prevent situations of stalling the API if
  // 10000 sub apps were provided.
  int sub_apps_total_limit = std::max(0, kSubAppsInstallLimitParam.Get());
  int over_the_limit =
      std::max(0, static_cast<int>(current_count + install_urls.size()) -
                      sub_apps_total_limit);
  if (over_the_limit > 0) {
    std::move(add_call_info.mojo_callback)
        .Run(base::unexpected(AddCallErrorCode::kTotalLimitExceeded));
    return;
  }

  auto parent_manifest_id = registrar.GetAppById(*parent_app_id)->manifest_id();
  CollectInstallData(add_call_id, std::move(install_urls), parent_manifest_id);
}

void SubAppsServiceImpl::CollectInstallData(
    int add_call_id,
    std::vector<GURL> requested_installs,
    webapps::ManifestId parent_manifest_id) {
  base::ConcurrentCallbacks<std::pair<GURL, std::unique_ptr<WebAppInstallInfo>>>
      concurrent;

  AddCallInfo& add_call_info =
      CHECK_DEREF(base::FindOrNull(add_call_info_, add_call_id));

  // Schedule data collection for each requested install
  for (const GURL& url_to_load : requested_installs) {
    std::optional<webapps::ManifestId> manifest_id =
        webapps::ManifestId::Create(GetUrlWithoutQueryAndRef(url_to_load));
    CHECK(manifest_id.has_value());

    // Check if app is the parent app itself
    if (*manifest_id == parent_manifest_id) {
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(url_to_load), /*manifest_id=*/std::nullopt,
          SubAppsServiceAddResultType::kRecursiveInstall));
      continue;
    }

    provider().scheduler().FetchInstallInfoFromInstallUrl(
        *manifest_id, url_to_load, parent_manifest_id,
        base::BindOnce(
            [](GURL install_url,
               std::unique_ptr<WebAppInstallInfo> install_info) {
              return std::pair(install_url, std::move(install_info));
            },
            url_to_load)
            .Then(concurrent.CreateCallback()));
  }

  std::move(concurrent)
      .Done(base::BindOnce(&SubAppsServiceImpl::ProcessInstallData,
                           weak_ptr_factory_.GetWeakPtr(), add_call_id));
}

void SubAppsServiceImpl::ProcessInstallData(
    int add_call_id,
    std::vector<std::pair<GURL, std::unique_ptr<WebAppInstallInfo>>>
        install_data) {
  AddCallInfo& add_call_info =
      CHECK_DEREF(base::FindOrNull(add_call_info_, add_call_id));
  const webapps::AppId* parent_app_id = GetAppId(render_frame_host());

  auto parent_manifest_id =
      provider().registrar_unsafe().GetAppById(*parent_app_id)->manifest_id();

  for (auto& [install_url, install_info] : install_data) {
    if (!install_info) {
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(install_url), /*manifest_id=*/std::nullopt,
          SubAppsServiceAddResultType::kInvalidManifest));
      continue;
    }

    auto manifest_id = install_info->manifest_id();

    // Check if app is the parent app itself
    if (manifest_id == parent_manifest_id) {
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(install_url), /*manifest_id=*/std::nullopt,
          SubAppsServiceAddResultType::kRecursiveInstall));
      continue;
    }

    // Check if app is already installed as a sub app
    if (provider().registrar_unsafe().AppMatches(
            GenerateAppIdFromManifestId(manifest_id),
            WebAppFilter::IsIsolatedSubApp())) {
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(install_url), /*manifest_id=*/std::nullopt,
          SubAppsServiceAddResultType::kAlreadyInstalled));
      continue;
    }

    install_info->parent_app_id = *parent_app_id;
    install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    bool scope_overlaps_with_other_apps = AppsScopesOverlap(
        install_info->scope, *parent_app_id, add_call_info.install_infos,
        provider().registrar_unsafe());

    if (scope_overlaps_with_other_apps) {
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(install_url), /*manifest_id=*/std::nullopt,
          SubAppsServiceAddResultType::kScopeOverlap));
      continue;
    }

    add_call_info.install_infos.emplace_back(std::move(install_info));
  }

  FinishAddCallOrShowInstallDialog(add_call_id);
}

void SubAppsServiceImpl::FinishAddCallOrShowInstallDialog(int add_call_id) {
  AddCallInfo& add_call_info =
      CHECK_DEREF(base::FindOrNull(add_call_info_, add_call_id));

  if (add_call_info.install_infos.empty()) {
    FinishAddCall(add_call_id, {});
    return;
  }

  switch (GetSubAppsContentSetting(render_frame_host())) {
    case CONTENT_SETTING_ALLOW:
      add_call_info.install_bypassed_prompt = true;
      ProcessDialogResponse(add_call_id, /*dialog_accepted=*/true);
      return;
    case CONTENT_SETTING_BLOCK:
      ProcessDialogResponse(add_call_id, /*dialog_accepted=*/false);
      return;
    case CONTENT_SETTING_ASK:
    default:
      const webapps::AppId* parent_app_id = GetAppId(render_frame_host());
      provider().ui_manager().ShowSubAppsInstallDialog(
          content::WebContents::FromRenderFrameHost(&render_frame_host()),
          add_call_info.install_infos, *parent_app_id,
          base::BindOnce(&SubAppsServiceImpl::ProcessDialogResponse,
                         weak_ptr_factory_.GetWeakPtr(), add_call_id));
      return;
  }
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

  AddCallInfo& add_call_info =
      CHECK_DEREF(base::FindOrNull(add_call_info_, add_call_id));

  std::move(add_call_info.mojo_callback)
      .Run(base::unexpected(AddCallErrorCode::kUserDeclined));
}

void SubAppsServiceImpl::ScheduleSubAppInstalls(int add_call_id) {
  AddCallInfo& add_call_info =
      CHECK_DEREF(base::FindOrNull(add_call_info_, add_call_id));

  // Schedule install for each install_info that was collected.
  base::ConcurrentCallbacks<SubAppInstallResult> concurrent;
  WebAppCommandScheduler& scheduler = provider().scheduler();
  for (auto& install_info : add_call_info.install_infos) {
    GURL install_url = install_info->install_url;
    webapps::ManifestId manifest_id = install_info->manifest_id();
    scheduler.InstallFromInfoWithParams(
        std::move(install_info), /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::SUB_APP,
        base::BindOnce(
            [](GURL install_url, webapps::ManifestId manifest_id,
               const webapps::AppId& app_id,
               webapps::InstallResultCode result_code) {
              return SubAppInstallResult(install_url, manifest_id, result_code);
            },
            install_url, manifest_id)
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
  AddCallInfo& add_call_info =
      CHECK_DEREF(base::FindOrNull(add_call_info_, add_call_id));

  for (const auto& [install_url, manifest_id, result_code] : install_results) {
    if (webapps::IsSuccess(result_code)) {
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(install_url), ConvertUrlToPath(manifest_id),
          SubAppsServiceAddResultType::kSuccess));
    } else {
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(install_url), /*manifest_id=*/std::nullopt,
          InstallResultCodeToAddResultType(result_code)));
    }
  }

  std::move(add_call_info.mojo_callback).Run(std::move(add_call_info.results));
}

void SubAppsServiceImpl::List(ListCallback result_callback) {
  if (!IsInstalledNonChildApp(render_frame_host())) {
    return std::move(result_callback)
        .Run(base::unexpected(SubAppsServiceResultCode::kWrongContext));
  }

  const WebAppRegistrar& registrar = provider().registrar_unsafe();
  std::vector<SubAppsServiceListResultEntryPtr> sub_apps_list;
  for (const webapps::AppId& sub_app_id :
       registrar.GetAllSubAppIds(*GetAppId(render_frame_host()))) {
    std::optional<webapps::ManifestId> manifest_id =
        registrar.GetAppManifestId(sub_app_id);
    if (!manifest_id.has_value()) {
      continue;
    }
    sub_apps_list.push_back(SubAppsServiceListResultEntry::New(
        ConvertUrlToPath(*manifest_id), registrar.GetAppShortName(sub_app_id)));
  }

  std::move(result_callback).Run(std::move(sub_apps_list));
}

void SubAppsServiceImpl::Remove(const std::vector<std::string>& manifest_ids,
                                RemoveCallback result_callback) {
  if (!IsInstalledNonChildApp(render_frame_host())) {
    return std::move(result_callback)
        .Run(base::unexpected(SubAppsServiceResultCode::kWrongContext));
  }

  // Take weak pointer early as this may get deleted by RemoveSubApp().
  base::WeakPtr<SubAppsServiceImpl> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  base::ConcurrentCallbacks<SubAppsServiceRemoveResultPtr> concurrent;
  for (const std::string& manifest_id : manifest_ids) {
    RemoveSubApp(manifest_id, concurrent.CreateCallback(),
                 GetAppId(render_frame_host()));
    // RemoveSubApp() may call ReportBadMessageAndDeleteThis() which deletes
    // `this`. The remaining callbacks in `concurrent` will be destroyed when
    // `concurrent` goes out of scope (it is a local), so they will not fire.
    // The weak_ptr-guarded .Done() callback below is also safe.
    if (!weak_ptr) {
      return;
    }
  }
  std::move(concurrent)
      .Done(base::BindOnce(&SubAppsServiceImpl::NotifyUninstall, weak_ptr,
                           std::move(result_callback)));
}

void SubAppsServiceImpl::RemoveSubApp(
    const std::string& manifest_id,
    base::OnceCallback<void(SubAppsServiceRemoveResultPtr)> callback,
    const webapps::AppId* calling_app_id) {
  // Convert `manifest_id` from path form to full URL form.
  ASSIGN_OR_RETURN(
      const GURL manifest_gurl,
      ConvertPathToUrl(manifest_id,
                       render_frame_host().GetLastCommittedOrigin()),
      // Compromised renderer, bail immediately (this call deletes *this).
      &SubAppsServiceImpl::ReportBadMessageAndDeleteThis, this);

  const webapps::AppId* parent_app_id = GetAppId(render_frame_host());
  if (!parent_app_id) {
    return ReportBadMessageAndDeleteThis("Parent app id is null");
  }

  WebAppRegistrar& registrar = provider().registrar_unsafe();

  std::optional<webapps::ManifestId> parent_manifest_id =
      registrar.GetAppManifestId(*parent_app_id);
  if (!parent_manifest_id.has_value()) {
    return ReportBadMessageAndDeleteThis("Invalid parent manifest id");
  }

  std::optional<webapps::ManifestId> valid_manifest_id =
      webapps::ManifestId::Create(manifest_gurl);
  if (!valid_manifest_id.has_value()) {
    return ReportBadMessageAndDeleteThis("Invalid manifest id");
  }
  webapps::AppId sub_app_id = GenerateAppIdFromManifestId(*valid_manifest_id);
  const WebApp* app = registrar.GetAppById(sub_app_id);

  // Verify that the app we're trying to remove exists, is installed and that
  // its parent_app is the one doing the current call.
  if (!app || !app->parent_app_id() ||
      *calling_app_id != *app->parent_app_id() ||
      !registrar.AppMatches(sub_app_id,
                            WebAppFilter::IsAppSurfaceableToUser())) {
    return std::move(callback).Run(SubAppsServiceRemoveResult::New(
        manifest_id, SubAppsServiceRemoveResultType::kNotFound));
  }

  // Note: While not possible today, if the sub app was installed via any other
  // management source (e.g. force install, user install, etc, preinstall),
  // then this doesn't uninstall the app.
  // - This could instead use the RemoveUserUninstallableManagements call,
  // which would make this effectively the same as the user trying to uninstall
  // the app using chrome://apps. This would NOT remove, say, the kPolicy
  // management source, as we must respect the policy force-installs.
  provider().scheduler().RemoveInstallManagementMaybeUninstall(
      sub_app_id, WebAppManagement::Type::kSubApp,
      webapps::WebappUninstallSource::kSubApp,
      base::BindOnce(
          [](std::string manifest_id,
             webapps::UninstallResultCode result_code) {
            SubAppsServiceRemoveResultType result =
                webapps::UninstallSucceeded(result_code)
                    ? SubAppsServiceRemoveResultType::kSuccess
                    : SubAppsServiceRemoveResultType::kGenericError;
            return SubAppsServiceRemoveResult::New(manifest_id, result);
          },
          manifest_id)
          .Then(std::move(callback)));
}

void SubAppsServiceImpl::NotifyUninstall(
    RemoveCallback result_callback,
    std::vector<SubAppsServiceRemoveResultPtr> remove_results) {
  int num_successful_uninstalls = std::ranges::count(
      remove_results, SubAppsServiceRemoveResultType::kSuccess,
      [](const auto& result) { return result->result_type; });

  // If any apps were uninstalled, notify the user.
  if (num_successful_uninstalls > 0) {
    WebAppRegistrar& registrar = provider().registrar_unsafe();
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
#if BUILDFLAG(IS_CHROMEOS)
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

void SubAppsServiceImpl::ReportAddMetricsAndRunCallback(
    const url::Origin& parent_origin,
    int add_call_id,
    AddCallback original_callback,
    AddResult result) {
  std::vector<IsolatedWebAppMetricsHelper::LogSubAppInstallResult>
      metric_results;

  if (!result.has_value()) {
    AddCallErrorCode error_code = result.error();
    metric_results.push_back(MapAddCallErrorCodeToMetric(error_code));
  } else {
    AddCallInfo& add_call_info =
        CHECK_DEREF(base::FindOrNull(add_call_info_, add_call_id));
    bool bypassed_prompt = add_call_info.install_bypassed_prompt;
    for (const auto& add_result : result.value()) {
      metric_results.push_back(MapAddResultTypeToMetricResult(
          add_result->result_type, bypassed_prompt));
    }
  }

  if (!metric_results.empty()) {
    IsolatedWebAppMetricsHelper::ReportSubAppInstallResults(parent_origin,
                                                            metric_results);
  }

  add_call_info_.erase(add_call_id);

  if (result.has_value()) {
    std::move(original_callback).Run(std::move(result.value()));
  } else {
    std::move(original_callback)
        .Run(base::unexpected(MapAddCallErrorCodeToMojo(result.error())));
  }
}

WebAppProvider& SubAppsServiceImpl::provider() const {
  return GetWebAppProvider(render_frame_host());
}

}  // namespace web_app
