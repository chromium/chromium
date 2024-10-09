// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/app_banner_manager.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/banners/app_banner_metrics.h"
#include "components/webapps/browser/banners/app_banner_settings_helper.h"
#include "components/webapps/browser/banners/install_banner_config.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/webapps_client.h"
#include "components/webapps/common/switches.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/installation/installation.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace webapps {
namespace {

bool IsManifestUrlChange(const InstallableData& result) {
  if (result.errors.empty()) {
    return false;
  }
  if (result.errors[0] != InstallableStatusCode::MANIFEST_URL_CHANGED) {
    return false;
  }
  return true;
}

}  // namespace

class AppBannerManager::StatusReporter {
 public:
  virtual ~StatusReporter() = default;

  // Reports |code| (via a mechanism which depends on the implementation).
  virtual void ReportStatus(InstallableStatusCode code) = 0;

  // Returns the WebappInstallSource to be used for this installation.
  virtual WebappInstallSource GetInstallSource(
      content::WebContents* web_contents,
      InstallTrigger trigger) = 0;
};

namespace {

int gTimeDeltaInDaysForTesting = 0;

InstallableParams ParamsToGetManifest() {
  InstallableParams params;
  params.check_eligibility = true;
  params.fetch_metadata = true;
  return params;
}

// Tracks installable status codes via an UMA histogram.
class TrackingStatusReporter : public AppBannerManager::StatusReporter {
 public:
  TrackingStatusReporter() = default;
  ~TrackingStatusReporter() override = default;

  // Records code via an UMA histogram.
  void ReportStatus(InstallableStatusCode code) override {
    // We only increment the histogram once per page load (and only if the
    // banner pipeline is triggered).
    if (!done_ && code != InstallableStatusCode::NO_ERROR_DETECTED) {
      TrackInstallableStatusCode(code);
    }

    done_ = true;
  }

  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override {
    return InstallableMetrics::GetInstallSource(web_contents, trigger);
  }

 private:
  bool done_ = false;
};

class NullStatusReporter : public AppBannerManager::StatusReporter {
 public:
  void ReportStatus(InstallableStatusCode code) override {
    // In general, NullStatusReporter::ReportStatus should not be called.
    // However, it may be called in cases where Stop is called without a
    // preceding call to RequestAppBanner e.g. because the WebContents is being
    // destroyed, a web app uninstalled, or the manifest url changing.
    DCHECK(code == InstallableStatusCode::NO_ERROR_DETECTED ||
           code == InstallableStatusCode::PIPELINE_RESTARTED ||
           code == InstallableStatusCode::MANIFEST_URL_CHANGED);
  }

  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override {
    NOTREACHED_IN_MIGRATION();
    return WebappInstallSource::COUNT;
  }
};

void TrackBeforeInstallEventPrompt(AppBannerManager::State state) {
  switch (state) {
    case AppBannerManager::State::SENDING_EVENT_GOT_EARLY_PROMPT:
      TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_EARLY_PROMPT);
      break;
    case AppBannerManager::State::PENDING_PROMPT_CANCELED:
      TrackBeforeInstallEvent(
          BEFORE_INSTALL_EVENT_PROMPT_CALLED_AFTER_PREVENT_DEFAULT);
      break;
    case AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED:
      TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_PROMPT_CALLED_NOT_CANCELED);
      break;
    default:
      break;
  }
}
}  // anonymous namespace

namespace test {
bool g_disable_banner_triggering_for_testing = false;
}

// static
AppBannerManager* AppBannerManager::FromWebContents(
    content::WebContents* web_contents) {
  return WebappsClient::Get()
             ? WebappsClient::Get()->GetAppBannerManager(web_contents)
             : nullptr;
}

// static
base::Time AppBannerManager::GetCurrentTime() {
  return base::Time::Now() + base::Days(gTimeDeltaInDaysForTesting);
}

// static
void AppBannerManager::SetTimeDeltaForTesting(int days) {
  gTimeDeltaInDaysForTesting = days;
}

std::optional<GURL> AppBannerManager::validated_url() const {
  return validated_url_.is_valid() ? std::make_optional(validated_url_)
                                   : std::nullopt;
}

InstallableWebAppCheckResult AppBannerManager::GetInstallableWebAppCheckResult()
    const {
  return installable_web_app_check_result_;
}

std::optional<InstallBannerConfig> AppBannerManager::GetCurrentBannerConfig()
    const {
  // Note: web_app_data_ being populated doesn't mean that this isn't a native
  // install banner config - that data is required before we determine if this
  // should be a native install, as the data to query that comes from the
  // manifest. The `mode_` is what tells this.
  if (!web_app_data_ || !validated_url_.is_valid()) {
    return std::nullopt;
  }
  return InstallBannerConfig(validated_url_, mode_, *web_app_data_,
                             native_app_data_);
}

std::optional<WebAppBannerData> AppBannerManager::GetCurrentWebAppBannerData()
    const {
  if (!web_app_data_ || !validated_url_.is_valid() ||
      mode_ == AppBannerMode::kNativeApp) {
    return std::nullopt;
  }
  return web_app_data_;
}

void AppBannerManager::RequestAppBanner() {
  DCHECK_EQ(State::INACTIVE, state_);

  if (!CanRequestAppBanner()) {
    return;
  }

  UpdateState(State::ACTIVE);

  // If we already have enough engagement, or require no engagement to trigger
  // the banner, the rest of the banner pipeline should operate as if the
  // engagement threshold has been met.
  if (!has_sufficient_engagement_ &&
      (AppBannerSettingsHelper::HasSufficientEngagement(0) ||
       AppBannerSettingsHelper::HasSufficientEngagement(
           GetSiteEngagementService()->GetScore(validated_url_)))) {
    has_sufficient_engagement_ = true;
  }

  status_reporter_ = std::make_unique<TrackingStatusReporter>();

  UpdateState(State::FETCHING_MANIFEST);
  manager_->GetData(ParamsToGetManifest(),
                    base::BindOnce(&AppBannerManager::OnDidGetManifest,
                                   GetWeakPtrForThisNavigation()));
}

void AppBannerManager::OnInstall(blink::mojom::DisplayMode display,
                                 bool set_current_web_app_not_installable) {
  TrackInstallDisplayMode(display);
  mojo::Remote<blink::mojom::InstallationService> installation_service;
  web_contents()->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      installation_service.BindNewPipeAndPassReceiver());
  DCHECK(installation_service);
  installation_service->OnInstall();

  // App has been installed (possibly by the user), page may no longer request
  // install prompt.
  receiver_.reset();
  if (set_current_web_app_not_installable) {
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
  }
}

void AppBannerManager::SendBannerAccepted() {
  if (event_.is_bound()) {
    event_->BannerAccepted(GetBannerType());
    event_.reset();
  }
}

void AppBannerManager::SendBannerDismissed() {
  if (event_.is_bound()) {
    event_->BannerDismissed();
    SendBannerPromptRequest();
  }
}

void AppBannerManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AppBannerManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

base::WeakPtr<AppBannerManager> AppBannerManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool AppBannerManager::TriggeringDisabledForTesting() const {
  return test::g_disable_banner_triggering_for_testing;
}

bool AppBannerManager::IsPromptAvailableForTesting() const {
  return receiver_.is_bound();
}

AppBannerManager::AppBannerManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      SiteEngagementObserver(site_engagement::SiteEngagementService::Get(
          web_contents->GetBrowserContext())),
      manager_(InstallableManager::FromWebContents(web_contents)),
      status_reporter_(std::make_unique<NullStatusReporter>()) {
  DCHECK(manager_);

  AppBannerSettingsHelper::UpdateFromFieldTrial();
}

AppBannerManager::~AppBannerManager() = default;

AppBannerManager::UrlType AppBannerManager::GetUrlType(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  // Don't start the banner flow unless the primary main frame has finished
  // loading. |render_frame_host| can be null during retry attempts.
  if (render_frame_host && !render_frame_host->IsInPrimaryMainFrame())
    return UrlType::kNotPrimaryFrame;

  // There is never a need to trigger a banner for a WebUI page, except
  // for PasswordManager WebUI.
  if (content::HasWebUIScheme(url) &&
      (url.host() != password_manager::kChromeUIPasswordManagerHost)) {
    return UrlType::kInvalidPrimaryFrameUrl;
  }

  return UrlType::kValidForBanner;
}

bool AppBannerManager::ShouldDeferToRelatedNonWebApp(
    const blink::mojom::Manifest& manifest) const {
  for (const auto& related_app : manifest.related_applications) {
    if (manifest.prefer_related_applications &&
        IsSupportedNonWebAppPlatform(
            related_app.platform.value_or(std::u16string()))) {
      return true;
    }
    if (IsRelatedNonWebAppInstalled(related_app)) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> AppBannerManager::GetWebOrNativeAppIdentifier()
    const {
  switch (mode_) {
    case AppBannerMode::kWebApp:
      if (!web_app_data_) {
        return std::nullopt;
      }
      return web_app_data_->manifest_id.spec();
    case AppBannerMode::kNativeApp:
      if (!native_app_data_) {
        return std::nullopt;
      }
      return native_app_data_->app_package;
  }
}

std::string AppBannerManager::GetBannerType() const {
  switch (mode_) {
    case AppBannerMode::kWebApp:
      return "web";
    case AppBannerMode::kNativeApp:
      return "play";
  }
}

bool AppBannerManager::HasSufficientEngagement() const {
  return has_sufficient_engagement_ || ShouldBypassEngagementChecks();
}

bool AppBannerManager::ShouldBypassEngagementChecks() const {
  return base::FeatureList::IsEnabled(
      webapps::features::kBypassAppBannerEngagementChecks);
}

void AppBannerManager::OnDidGetManifest(const InstallableData& data) {
  // The pipeline will be restarted from DidUpdateWebManifestURL.
  if (IsManifestUrlChange(data)) {
    return;
  }
  if (state() != State::FETCHING_MANIFEST) {
    return;
  }
  UpdateState(State::ACTIVE);

  // An empty manifest indicates some kind of unrecoverable error occurred.
  if (blink::IsEmptyManifest(*data.manifest)) {
    CHECK(!data.errors.empty());
    Stop(data.GetFirstError());
    return;
  }

  CHECK(data.manifest->id.is_valid());
  web_app_data_.emplace(data.manifest->id, data.manifest->Clone(),
                        data.web_page_metadata->Clone(), *(data.manifest_url));
  WebappsClient::Get()->OnManifestSeen(web_contents()->GetBrowserContext(),
                                       *data.manifest);

  PerformInstallableChecks();
}

void AppBannerManager::PerformInstallableChecks() {
  CHECK(web_app_data_);
  if (ShouldDoNativeAppCheck(web_app_data_->manifest())) {
    UpdateState(State::FETCHING_NATIVE_DATA);
    mode_ = AppBannerMode::kNativeApp;
    DoNativeAppInstallableCheck(
        web_contents(), validated_url_, web_app_data_->manifest(),
        base::BindOnce(&AppBannerManager::OnNativeAppInstallableCheckComplete,
                       weak_factory_for_this_navigation_.GetWeakPtr()));
    return;
  }
  mode_ = AppBannerMode::kWebApp;

  PerformInstallableWebAppCheck();
}

void AppBannerManager::OnNativeAppInstallableCheckComplete(
    base::expected<NativeAppBannerData, InstallableStatusCode> result) {
  if (state_ != State::FETCHING_NATIVE_DATA) {
    return;
  }
  if (!result.has_value()) {
    Stop(result.error());
    return;
  }
  CHECK(mode_ == AppBannerMode::kNativeApp);

  native_app_data_.emplace(result.value());

  // If we triggered the installability check on page load, then it's possible
  // we don't have enough engagement yet. If that's the case, return here but
  // don't call Terminate(). We wait for OnEngagementEvent to tell us that we
  // should trigger.
  if (!HasSufficientEngagement()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  SendBannerPromptRequest();
}

void AppBannerManager::PerformInstallableWebAppCheck() {
  CHECK(mode_ == AppBannerMode::kWebApp);
  CHECK(state_ == State::ACTIVE);
  CHECK(web_app_data_);

  base::expected<void, InstallableStatusCode>
      can_run_web_app_installable_checks =
          CanRunWebAppInstallableChecks(web_app_data_->manifest());
  if (!can_run_web_app_installable_checks.has_value()) {
    Stop(can_run_web_app_installable_checks.error());
    return;
  }

  // Fetch and verify the other required information.
  UpdateState(State::PENDING_INSTALLABLE_CHECK);
  manager_->GetData(
      ParamsToPerformInstallableWebAppCheck(),
      base::BindOnce(&AppBannerManager::OnDidPerformInstallableWebAppCheck,
                     weak_factory_for_this_navigation_.GetWeakPtr()));
}

void AppBannerManager::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  CHECK(mode_ == AppBannerMode::kWebApp);
  CHECK(web_app_data_);
  // The pipeline will be restarted from DidUpdateWebManifestURL.
  if (IsManifestUrlChange(data)) {
    return;
  }
  if (state_ != State::PENDING_INSTALLABLE_CHECK) {
    return;
  }

  UpdateState(State::ACTIVE);
  if (data.installable_check_passed) {
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED);
  }

  bool is_installable = data.errors.empty();

  if (!is_installable) {
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
    Stop(data.GetFirstError());
    return;
  }
  OnWebAppInstallableCheckedNoErrors(data.manifest->id);

  WebappsClient* client = WebappsClient::Get();
  if (client->DoesNewWebAppConflictWithExistingInstallation(
          web_contents()->GetBrowserContext(),
          web_app_data_->manifest().start_url, web_app_data_->manifest_id)) {
    TrackDisplayEvent(DISPLAY_EVENT_INSTALLED_PREVIOUSLY);
    SetInstallableWebAppCheckResult(
        InstallableWebAppCheckResult::kNo_AlreadyInstalled);
    Stop(InstallableStatusCode::ALREADY_INSTALLED);
    return;
  }

  // This must be true because `is_installable` is true (no errors).
  DCHECK(data.installable_check_passed);
  DCHECK(!data.primary_icon_url->is_empty());
  DCHECK(data.primary_icon);

  web_app_data_->primary_icon_url = *data.primary_icon_url;
  web_app_data_->primary_icon = *data.primary_icon;
  web_app_data_->has_maskable_primary_icon = data.has_maskable_primary_icon;
  web_app_data_->screenshots = *(data.screenshots);

  if (ShouldDeferToRelatedNonWebApp(web_app_data_->manifest())) {
    SetInstallableWebAppCheckResult(
        InstallableWebAppCheckResult::kYes_ByUserRequest);
    Stop(InstallableStatusCode::PREFER_RELATED_APPLICATIONS);
    return;
  }

  SetInstallableWebAppCheckResult(
      InstallableWebAppCheckResult::kYes_Promotable);
  CheckSufficientEngagement();
}

void AppBannerManager::CheckSufficientEngagement() {
  // If we triggered the installability check on page load, then it's
  // possible we don't have enough engagement yet. If that's the case,
  // return here but don't call Terminate(). We wait for OnEngagementEvent
  // to tell us that we should trigger.
  if (!HasSufficientEngagement()) {
    UpdateState(State::PENDING_ENGAGEMENT);
    return;
  }

  SendBannerPromptRequest();
}

void AppBannerManager::ReportStatus(InstallableStatusCode code) {
  DCHECK(status_reporter_);
  status_reporter_->ReportStatus(code);
}

void AppBannerManager::ResetBindings() {
  receiver_.reset();
  event_.reset();
}

void AppBannerManager::ResetCurrentPageDataInternal() {
  InvalidateWeakPtrsForThisNavigation();
  load_finished_ = false;
  has_sufficient_engagement_ = false;
  active_media_players_.clear();
  web_app_data_.reset();
  native_app_data_.reset();
  mode_ = AppBannerMode::kWebApp;
  validated_url_ = GURL();
  UpdateState(State::INACTIVE);
  SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kUnknown);
  ResetCurrentPageData();
}

void AppBannerManager::Terminate(InstallableStatusCode code) {
  switch (state_) {
    case State::PENDING_PROMPT_CANCELED:
      TrackBeforeInstallEvent(
          BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_AFTER_PREVENT_DEFAULT);
      break;
    case State::PENDING_PROMPT_NOT_CANCELED:
      TrackBeforeInstallEvent(
          BEFORE_INSTALL_EVENT_PROMPT_NOT_CALLED_NOT_CANCELLED);
      break;
    case State::PENDING_ENGAGEMENT:
      if (!has_sufficient_engagement_)
        TrackDisplayEvent(DISPLAY_EVENT_NOT_VISITED_ENOUGH);
      break;
    default:
      break;
  }

  Stop(code);
}

InstallableStatusCode AppBannerManager::TerminationCodeFromState() const {
  switch (state_) {
    case State::PENDING_PROMPT_CANCELED:
    case State::PENDING_PROMPT_NOT_CANCELED:
      return InstallableStatusCode::RENDERER_CANCELLED;
    case State::PENDING_ENGAGEMENT:
      return has_sufficient_engagement_
                 ? InstallableStatusCode::NO_ERROR_DETECTED
                 : InstallableStatusCode::INSUFFICIENT_ENGAGEMENT;
    case State::FETCHING_MANIFEST:
      return InstallableStatusCode::WAITING_FOR_MANIFEST;
    case State::FETCHING_NATIVE_DATA:
      return InstallableStatusCode::WAITING_FOR_NATIVE_DATA;
    case State::PENDING_INSTALLABLE_CHECK:
      return InstallableStatusCode::WAITING_FOR_INSTALLABLE_CHECK;
    case State::ACTIVE:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
    case State::INACTIVE:
    case State::COMPLETE:
      break;
  }
  return InstallableStatusCode::NO_ERROR_DETECTED;
}

void AppBannerManager::SetInstallableWebAppCheckResult(
    InstallableWebAppCheckResult result) {
  if (installable_web_app_check_result_ == result) {
    return;
  }
  installable_web_app_check_result_ = result;

  // First save the last result as long as the state isn't kUnknown.
  if (web_app_data_ && result != InstallableWebAppCheckResult::kUnknown) {
    last_known_result_ = std::make_pair(
        std::make_unique<WebAppBannerData>(*web_app_data_), result);
  }
  // Second, update the install animation.
  switch (result) {
    case InstallableWebAppCheckResult::kUnknown:
      break;
    case InstallableWebAppCheckResult::kYes_Promotable:
      CHECK(web_app_data_);
      install_animation_pending_ =
          AppBannerSettingsHelper::CanShowInstallTextAnimation(
              web_contents(), web_app_data_->manifest().scope);
      break;
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
      CHECK(web_app_data_);
      [[fallthrough]];
    case InstallableWebAppCheckResult::kNo:
      install_animation_pending_ = false;
      break;
  }

  for (Observer& observer : observer_list_) {
    observer.OnInstallableWebAppStatusUpdated(result, web_app_data_);
  }
}

void AppBannerManager::RecheckInstallabilityForLoadedPage() {
  if (state_ == State::INACTIVE)
    return;

  if (state_ != State::COMPLETE) {
    Stop(InstallableStatusCode::PIPELINE_RESTARTED);
  }

  UpdateState(State::INACTIVE);
  RequestAppBanner();
}

void AppBannerManager::Stop(InstallableStatusCode code) {
  ReportStatus(code);

  InvalidateWeakPtrsForThisNavigation();
  if (installable_web_app_check_result_ ==
      InstallableWebAppCheckResult::kUnknown) {
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
  }
  ResetBindings();
  UpdateState(State::COMPLETE);
  status_reporter_ = std::make_unique<NullStatusReporter>();
}

void AppBannerManager::SendBannerPromptRequest() {
  std::optional<InstallBannerConfig> install_config = GetCurrentBannerConfig();
  CHECK(install_config.has_value());
  // Record that the banner could be shown at this point, if the triggering
  // heuristic allowed.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), install_config.value(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, GetCurrentTime());

  UpdateState(State::SENDING_EVENT);
  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_CREATED);

  // Any existing binding is invalid when we send a new beforeinstallprompt.
  ResetBindings();

  mojo::Remote<blink::mojom::AppBannerController> controller;
  web_contents()->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      controller.BindNewPipeAndPassReceiver());

  // Get a raw controller pointer before we move out of the smart pointer to
  // avoid crashing with MSVC's order of evaluation.
  blink::mojom::AppBannerController* controller_ptr = controller.get();
  controller_ptr->BannerPromptRequest(
      receiver_.BindNewPipeAndPassRemote(), event_.BindNewPipeAndPassReceiver(),
      {GetBannerType()},
      base::BindOnce(&AppBannerManager::OnBannerPromptReply,
                     GetWeakPtrForThisNavigation(), install_config.value(),
                     std::move(controller)));
}

void AppBannerManager::UpdateState(State state) {
  state_ = state;
}

void AppBannerManager::DidFinishNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }

  if (state_ != State::COMPLETE && state_ != State::INACTIVE) {
    Terminate(TerminationCodeFromState());
  }
  ResetCurrentPageDataInternal();

  if (handle->IsServedFromBackForwardCache()) {
    UrlType url_type =
        GetUrlType(/*render_frame_host=*/nullptr, handle->GetURL());
    if (url_type != UrlType::kValidForBanner) {
      return;
    }

    load_finished_ = true;
    validated_url_ = handle->GetURL();
    RequestAppBanner();
  }
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (TriggeringDisabledForTesting()) {
    return;
  }

  UrlType url_type = GetUrlType(render_frame_host, validated_url);
  if (url_type != UrlType::kValidForBanner) {
    return;
  }

  load_finished_ = true;
  validated_url_ = validated_url;

  // Start the pipeline immediately if we haven't already started it.
  if (state_ == State::INACTIVE)
    RequestAppBanner();
}

void AppBannerManager::DidFailLoad(content::RenderFrameHost* render_frame_host,
                                   const GURL& validated_url,
                                   int error_code) {
  // This is called with `net::ERR_ABORTED` if the developer manually stops the
  // loading of the page. The pipeline still need to run if this occurs.
  if (error_code == net::ERR_ABORTED) {
    DidFinishLoad(render_frame_host, validated_url);
  }
}

void AppBannerManager::DidUpdateWebManifestURL(
    content::RenderFrameHost* target_frame,
    const GURL& manifest_url) {
  if (state_ == State::INACTIVE ||
      (state_ == State::COMPLETE && manifest_url.is_empty()) ||
      !target_frame->IsInPrimaryMainFrame()) {
    return;
  }
  Terminate(manifest_url.is_empty()
                ? InstallableStatusCode::NO_MANIFEST
                : InstallableStatusCode::MANIFEST_URL_CHANGED);
  if (!manifest_url.is_empty()) {
    RecheckInstallabilityForLoadedPage();
  }
}

void AppBannerManager::MediaStartedPlaying(const MediaPlayerInfo& media_info,
                                           const content::MediaPlayerId& id) {
  active_media_players_.push_back(id);
}

void AppBannerManager::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const content::MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  std::erase(active_media_players_, id);
}

void AppBannerManager::WebContentsDestroyed() {
  Terminate(TerminationCodeFromState());
  manager_ = nullptr;
}

void AppBannerManager::OnEngagementEvent(
    content::WebContents* contents,
    const GURL& url,
    double score,
    double old_score,
    site_engagement::EngagementType /*type*/,
    const std::optional<webapps::AppId>& /*app_id*/) {
  if (TriggeringDisabledForTesting() || ShouldBypassEngagementChecks()) {
    return;
  }

  // Only trigger a banner using site engagement if:
  //  1. engagement increased for the web contents which we are attached to; and
  //  2. there are no currently active media players; and
  //  3. we have accumulated sufficient engagement.
  if (web_contents() == contents && active_media_players_.empty() &&
      AppBannerSettingsHelper::HasSufficientEngagement(score)) {
    has_sufficient_engagement_ = true;

    if (state_ == State::PENDING_ENGAGEMENT) {
      // We have already finished the installability eligibility checks. Proceed
      // directly to sending the banner prompt request.
      UpdateState(State::ACTIVE);
      SendBannerPromptRequest();
    } else if (load_finished_ && validated_url_ == url &&
               state_ == State::INACTIVE) {
      // This performs some simple tests and starts async checks to test
      // installability. It should be safe to start in response to user input.
      // Don't call if we're already working on processing a banner request.
      RequestAppBanner();
    }
  }
}

bool AppBannerManager::IsRunning() const {
  switch (state_) {
    case State::INACTIVE:
    case State::PENDING_PROMPT_CANCELED:
    case State::PENDING_PROMPT_NOT_CANCELED:
    case State::PENDING_ENGAGEMENT:
    case State::COMPLETE:
      return false;
    case State::ACTIVE:
    case State::FETCHING_MANIFEST:
    case State::FETCHING_NATIVE_DATA:
    case State::PENDING_INSTALLABLE_CHECK:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
      return true;
  }
  return false;
}

// static
std::u16string AppBannerManager::GetInstallableWebAppName(
    content::WebContents* web_contents) {
  AppBannerManager* manager = FromWebContents(web_contents);
  if (!manager)
    return std::u16string();
  switch (manager->installable_web_app_check_result_) {
    case InstallableWebAppCheckResult::kUnknown:
    case InstallableWebAppCheckResult::kNo:
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      return std::u16string();
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
    case InstallableWebAppCheckResult::kYes_Promotable:
      auto config = manager->GetCurrentBannerConfig();
      CHECK(config);
      return config->GetWebOrNativeAppName();
  }
}
// static
std::string AppBannerManager::GetInstallableWebAppManifestId(
    content::WebContents* web_contents) {
  AppBannerManager* manager = FromWebContents(web_contents);
  if (!manager)
    return std::string();
  switch (manager->installable_web_app_check_result_) {
    case InstallableWebAppCheckResult::kUnknown:
    case InstallableWebAppCheckResult::kNo:
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      return std::string();
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
    case InstallableWebAppCheckResult::kYes_Promotable:
      CHECK(manager->web_app_data_);
      return manager->web_app_data_->manifest().id.spec();
  }
}

bool AppBannerManager::IsProbablyPromotableWebApp(
    bool ignore_existing_installations) const {
  // First check the current status.
  switch (installable_web_app_check_result_) {
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      return ignore_existing_installations;
    case InstallableWebAppCheckResult::kNo:
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
      return false;
    case InstallableWebAppCheckResult::kYes_Promotable:
      return true;
    case InstallableWebAppCheckResult::kUnknown:
      break;
  }
  // If the current status is unknown, try to deduce from the last result if the
  // last  result has an overlapping scope with the current url.
  if (last_known_result_ == std::nullopt) {
    return false;
  }
  bool last_result_overlaps_current_url =
      base::StartsWith(web_contents()->GetLastCommittedURL().spec(),
                       last_known_result_->first->manifest().scope.spec(),
                       base::CompareCase::SENSITIVE);
  if (!last_result_overlaps_current_url) {
    return false;
  }
  switch (last_known_result_->second) {
    case InstallableWebAppCheckResult::kUnknown:
    case InstallableWebAppCheckResult::kNo:
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
      return false;
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      return ignore_existing_installations;
    case InstallableWebAppCheckResult::kYes_Promotable:
      return true;
  }
}

bool AppBannerManager::IsPromotableWebApp() const {
  switch (installable_web_app_check_result_) {
    case InstallableWebAppCheckResult::kUnknown:
    case InstallableWebAppCheckResult::kNo:
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
      return false;
    case InstallableWebAppCheckResult::kYes_Promotable:
      return true;
  }
}

bool AppBannerManager::MaybeConsumeInstallAnimation() {
  DCHECK(IsProbablyPromotableWebApp());
  if (!install_animation_pending_)
    return false;
  if (!last_known_result_) {
    return false;
  }
  AppBannerSettingsHelper::RecordInstallTextAnimationShown(
      web_contents(), last_known_result_->first->manifest().scope);
  install_animation_pending_ = false;
  return true;
}

void AppBannerManager::OnBannerPromptReply(
    const InstallBannerConfig& install_config,
    mojo::Remote<blink::mojom::AppBannerController> controller,
    blink::mojom::AppBannerPromptReply reply) {
  // The renderer might have requested the prompt to be canceled. They may
  // request that it is redisplayed later, so don't Terminate() here. However,
  // log that the cancelation was requested, so Terminate() can be called if a
  // redisplay isn't asked for.
  //
  // If the redisplay request has not been received already, we stop here and
  // wait for the prompt function to be called. If the redisplay request has
  // already been received before cancel was sent (e.g. if redisplay was
  // requested in the beforeinstallprompt event handler), we keep going and show
  // the banner immediately.
  bool event_canceled = reply == blink::mojom::AppBannerPromptReply::CANCEL;
  if (event_canceled) {
    TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_PREVENT_DEFAULT_CALLED);
    if (ShouldBypassEngagementChecks()) {
      web_contents()->GetPrimaryMainFrame()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          "Banner not shown: beforeinstallpromptevent.preventDefault() called. "
          "The page must call beforeinstallpromptevent.prompt() to show the "
          "banner.");
    }
  }

  if (state_ == State::SENDING_EVENT) {
    if (!event_canceled) {
      MaybeShowAmbientBadge(install_config);
      UpdateState(State::PENDING_PROMPT_NOT_CANCELED);
    } else {
      UpdateState(State::PENDING_PROMPT_CANCELED);
    }
    return;
  }

  DCHECK_EQ(State::SENDING_EVENT_GOT_EARLY_PROMPT, state_);

  ShowBannerForCurrentPageState();
}

void AppBannerManager::ShowBannerForCurrentPageState() {
  // The banner is only shown if the site explicitly requests it to be shown.
  DCHECK_NE(State::SENDING_EVENT, state_);

  content::WebContents* contents = web_contents();
  WebappInstallSource install_source;

  TrackBeforeInstallEventPrompt(state_);

  install_source =
      status_reporter_->GetInstallSource(contents, InstallTrigger::API);

  DCHECK(web_app_data_);
  DCHECK(!web_app_data_->manifest_url.is_empty());
  DCHECK(!blink::IsEmptyManifest(web_app_data_->manifest()));
  switch (mode_) {
    case AppBannerMode::kNativeApp:
      DCHECK(native_app_data_);
      DCHECK(!native_app_data_->primary_icon_url.is_empty());
      DCHECK(!native_app_data_->primary_icon.drawsNothing());
      break;
    case AppBannerMode::kWebApp:
      DCHECK(!web_app_data_->primary_icon_url.is_empty());
      DCHECK(!web_app_data_->primary_icon.drawsNothing());
      break;
  }
  std::optional<InstallBannerConfig> config = GetCurrentBannerConfig();
  CHECK(config);
  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_COMPLETE);
  ShowBannerUi(install_source, config.value());
  ReportStatus(InstallableStatusCode::SHOWING_APP_INSTALLATION_DIALOG);
  UpdateState(State::COMPLETE);
}

void AppBannerManager::DisplayAppBanner() {
  // Prevent this from being called multiple times on the same connection.
  receiver_.reset();

  if (state_ == State::PENDING_PROMPT_CANCELED ||
      state_ == State::PENDING_PROMPT_NOT_CANCELED) {
    ShowBannerForCurrentPageState();
  } else if (state_ == State::SENDING_EVENT) {
    // Log that the prompt request was made for when we get the prompt reply.
    UpdateState(State::SENDING_EVENT_GOT_EARLY_PROMPT);
  }
}

}  // namespace webapps
