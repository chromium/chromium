// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/banners/app_banner_manager.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
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
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
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
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
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
  if (result.errors[0] != MANIFEST_URL_CHANGED) {
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
  return params;
}

// Logs installable status codes to the console.
class ConsoleStatusReporter : public AppBannerManager::StatusReporter {
 public:
  // Constructs a ConsoleStatusReporter which logs to the devtools console
  // attached to |web_contents|.
  explicit ConsoleStatusReporter(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  // Logs an error message corresponding to |code| to the devtools console.
  void ReportStatus(InstallableStatusCode code) override {
    LogToConsole(web_contents_, code,
                 blink::mojom::ConsoleMessageLevel::kError);
  }

  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override {
    return WebappInstallSource::DEVTOOLS;
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

// Tracks installable status codes via an UMA histogram.
class TrackingStatusReporter : public AppBannerManager::StatusReporter {
 public:
  TrackingStatusReporter() = default;
  ~TrackingStatusReporter() override = default;

  // Records code via an UMA histogram.
  void ReportStatus(InstallableStatusCode code) override {
    // We only increment the histogram once per page load (and only if the
    // banner pipeline is triggered).
    if (!done_ && code != NO_ERROR_DETECTED)
      TrackInstallableStatusCode(code);

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
    // destroyed or web app uninstalled. In that case, code should always be
    // NO_ERROR_DETECTED or PIPELINE_RESTARTED.
    DCHECK(code == NO_ERROR_DETECTED || code == PIPELINE_RESTARTED);
  }

  WebappInstallSource GetInstallSource(content::WebContents* web_contents,
                                       InstallTrigger trigger) override {
    NOTREACHED();
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

void AppBannerManager::RequestAppBanner(const GURL& validated_url) {
  DCHECK_EQ(State::INACTIVE, state_);

  UpdateState(State::ACTIVE);

  // If we already have enough engagement, or require no engagement to trigger
  // the banner, the rest of the banner pipeline should operate as if the
  // engagement threshold has been met.
  if (!has_sufficient_engagement_ &&
      (AppBannerSettingsHelper::HasSufficientEngagement(0) ||
       AppBannerSettingsHelper::HasSufficientEngagement(
           GetSiteEngagementService()->GetScore(validated_url)))) {
    has_sufficient_engagement_ = true;
  }

  if (ShouldBypassEngagementChecks())
    status_reporter_ = std::make_unique<ConsoleStatusReporter>(web_contents());
  else
    status_reporter_ = std::make_unique<TrackingStatusReporter>();

  if (validated_url_.is_empty())
    validated_url_ = validated_url;

  UpdateState(State::FETCHING_MANIFEST);
  manager_->GetData(ParamsToGetManifest(),
                    base::BindOnce(&AppBannerManager::OnDidGetManifest,
                                   GetWeakPtrForThisNavigation()));
}

void AppBannerManager::OnInstall(blink::mojom::DisplayMode display) {
  TrackInstallDisplayMode(display);
  mojo::Remote<blink::mojom::InstallationService> installation_service;
  web_contents()->GetPrimaryMainFrame()->GetRemoteInterfaces()->GetInterface(
      installation_service.BindNewPipeAndPassReceiver());
  DCHECK(installation_service);
  installation_service->OnInstall();

  // App has been installed (possibly by the user), page may no longer request
  // install prompt.
  receiver_.reset();
}

void AppBannerManager::SendBannerAccepted() {
  if (event_.is_bound()) {
    event_->BannerAccepted(GetBannerType());
    event_.reset();
  }
}

void AppBannerManager::SendBannerDismissed() {
  if (event_.is_bound())
    event_->BannerDismissed();

  SendBannerPromptRequest();
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

AppBannerManager::InstallableWebAppCheckResult
AppBannerManager::GetInstallableWebAppCheckResultForTesting() {
  return installable_web_app_check_result_;
}

AppBannerManager::AppBannerManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      SiteEngagementObserver(site_engagement::SiteEngagementService::Get(
          web_contents->GetBrowserContext())),
      manager_(InstallableManager::FromWebContents(web_contents)),
      manifest_(blink::mojom::Manifest::New()),
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

bool AppBannerManager::CheckIfShouldShowBanner() {
  if (ShouldBypassEngagementChecks()) {
    return true;
  }
  if (GetAppIdentifier().empty()) {
    Stop(PACKAGE_NAME_OR_START_URL_EMPTY);
    return false;
  }
  return true;
}

bool AppBannerManager::ShouldDeferToRelatedNonWebApp() const {
  for (const auto& related_app : manifest().related_applications) {
    if (manifest().prefer_related_applications &&
        IsSupportedNonWebAppPlatform(
            related_app.platform.value_or(std::u16string()))) {
      return true;
    }
    if (IsRelatedNonWebAppInstalled(related_app))
      return true;
  }
  return false;
}

std::string AppBannerManager::GetAppIdentifier() {
  DCHECK(!blink::IsEmptyManifest(manifest()));
  return manifest().start_url.spec();
}

std::u16string AppBannerManager::GetAppName() const {
  return manifest().name.value_or(std::u16string());
}

const blink::mojom::Manifest& AppBannerManager::manifest() const {
  DCHECK(manifest_);
  return *manifest_;
}

std::string AppBannerManager::GetBannerType() {
  return "web";
}

bool AppBannerManager::HasSufficientEngagement() const {
  return has_sufficient_engagement_ || ShouldBypassEngagementChecks();
}

bool AppBannerManager::ShouldBypassEngagementChecks() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kBypassAppBannerEngagementChecks);
}

bool AppBannerManager::ShouldAllowWebAppReplacementInstall() {
  return false;
}

void AppBannerManager::OnDidGetManifest(const InstallableData& data) {
  // The pipeline will be restarted from DidUpdateWebManifestURL.
  if (IsManifestUrlChange(data)) {
    return;
  }
  UpdateState(State::ACTIVE);
  if (!data.NoBlockingErrors()) {
    Stop(data.errors[0]);
    return;
  }

  DCHECK(!data.manifest_url->is_empty());
  DCHECK(!blink::IsEmptyManifest(*data.manifest));

  manifest_url_ = *(data.manifest_url);
  manifest_ = data.manifest->Clone();
  manifest_id_ = blink::GetIdFromManifest(manifest());

  // Skip checks for PasswordManager WebUI page.
  if (content::HasWebUIScheme(validated_url_) &&
      (validated_url_.host() ==
       password_manager::kChromeUIPasswordManagerHost)) {
    if (IsWebAppConsideredInstalled()) {
      TrackDisplayEvent(DISPLAY_EVENT_INSTALLED_PREVIOUSLY);
      SetInstallableWebAppCheckResult(
          InstallableWebAppCheckResult::kNo_AlreadyInstalled);
      Stop(ALREADY_INSTALLED);
    } else {
      SetInstallableWebAppCheckResult(
          InstallableWebAppCheckResult::kYes_Promotable);
      Stop(NO_ERROR_DETECTED);
    }
    return;
  }

  PerformInstallableChecks();
}

InstallableParams AppBannerManager::ParamsToPerformInstallableWebAppCheck() {
  InstallableParams params;
  params.valid_primary_icon = true;
  params.valid_manifest = true;
  params.fetch_screenshots = true;

  return params;
}

void AppBannerManager::PerformInstallableChecks() {
  PerformInstallableWebAppCheck();
}

void AppBannerManager::PerformInstallableWebAppCheck() {
  if (!CheckIfShouldShowBanner())
    return;

  // Fetch and verify the other required information.
  UpdateState(State::PENDING_INSTALLABLE_CHECK);
  manager_->GetData(
      ParamsToPerformInstallableWebAppCheck(),
      base::BindOnce(&AppBannerManager::OnDidPerformInstallableWebAppCheck,
                     GetWeakPtrForThisNavigation()));
}

void AppBannerManager::OnDidPerformInstallableWebAppCheck(
    const InstallableData& data) {
  // The pipeline will be restarted from DidUpdateWebManifestURL.
  if (IsManifestUrlChange(data)) {
    return;
  }

  UpdateState(State::ACTIVE);
  if (data.valid_manifest)
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_REQUESTED);

  bool is_installable = data.NoBlockingErrors();

  if (!is_installable) {
    DCHECK(!data.errors.empty());
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
    Stop(data.errors[0]);
    return;
  }

  if (IsWebAppConsideredInstalled() && !ShouldAllowWebAppReplacementInstall()) {
    TrackDisplayEvent(DISPLAY_EVENT_INSTALLED_PREVIOUSLY);
    SetInstallableWebAppCheckResult(
        InstallableWebAppCheckResult::kNo_AlreadyInstalled);
    Stop(ALREADY_INSTALLED);
    return;
  }

  if (ShouldDeferToRelatedNonWebApp()) {
    SetInstallableWebAppCheckResult(
        InstallableWebAppCheckResult::kYes_ByUserRequest);
    Stop(PREFER_RELATED_APPLICATIONS);
    return;
  }

  DCHECK(data.valid_manifest);
  DCHECK(!data.primary_icon_url->is_empty());
  DCHECK(data.primary_icon);

  primary_icon_url_ = *data.primary_icon_url;
  primary_icon_ = *data.primary_icon;
  has_maskable_primary_icon_ = data.has_maskable_primary_icon;
  screenshots_ = *(data.screenshots);

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

void AppBannerManager::RecordDidShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, GetCurrentTime());
}

void AppBannerManager::ReportStatus(InstallableStatusCode code) {
  DCHECK(status_reporter_);
  status_reporter_->ReportStatus(code);
}

void AppBannerManager::ResetBindings() {
  receiver_.reset();
  event_.reset();
}

void AppBannerManager::ResetCurrentPageData() {
  load_finished_ = false;
  has_sufficient_engagement_ = false;
  active_media_players_.clear();
  manifest_ = blink::mojom::Manifest::New();
  manifest_url_ = GURL();
  manifest_id_ = GURL();
  validated_url_ = GURL();
  UpdateState(State::INACTIVE);
  SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kUnknown);
  install_path_tracker_.Reset();
  screenshots_.clear();
}

void AppBannerManager::Terminate() {
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

  Stop(TerminationCode());
}

InstallableStatusCode AppBannerManager::TerminationCode() const {
  switch (state_) {
    case State::PENDING_PROMPT_CANCELED:
    case State::PENDING_PROMPT_NOT_CANCELED:
      return RENDERER_CANCELLED;
    case State::PENDING_ENGAGEMENT:
      return has_sufficient_engagement_ ? NO_ERROR_DETECTED
                                        : INSUFFICIENT_ENGAGEMENT;
    case State::FETCHING_MANIFEST:
      return WAITING_FOR_MANIFEST;
    case State::FETCHING_NATIVE_DATA:
      return WAITING_FOR_NATIVE_DATA;
    case State::PENDING_INSTALLABLE_CHECK:
      return WAITING_FOR_INSTALLABLE_CHECK;
    case State::ACTIVE:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
    case State::INACTIVE:
    case State::COMPLETE:
      break;
  }
  return NO_ERROR_DETECTED;
}

void AppBannerManager::SetInstallableWebAppCheckResult(
    InstallableWebAppCheckResult result) {
  if (installable_web_app_check_result_ == result)
    return;

  installable_web_app_check_result_ = result;

  switch (result) {
    case InstallableWebAppCheckResult::kUnknown:
      break;
    case InstallableWebAppCheckResult::kYes_Promotable:
      last_promotable_web_app_scope_ = manifest().scope;
      DCHECK(!last_promotable_web_app_scope_.is_empty());
      last_already_installed_web_app_scope_ = GURL();
      install_animation_pending_ =
          AppBannerSettingsHelper::CanShowInstallTextAnimation(
              web_contents(), last_promotable_web_app_scope_);
      break;
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      last_already_installed_web_app_scope_ = manifest().scope;
      DCHECK(!last_already_installed_web_app_scope_.is_empty());
      last_promotable_web_app_scope_ = GURL();
      install_animation_pending_ = false;
      break;
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
    case InstallableWebAppCheckResult::kNo:
      last_promotable_web_app_scope_ = GURL();
      last_already_installed_web_app_scope_ = GURL();
      install_animation_pending_ = false;
      break;
  }

  for (Observer& observer : observer_list_)
    observer.OnInstallableWebAppStatusUpdated();
}

void AppBannerManager::RecheckInstallabilityForLoadedPage() {
  if (state_ == State::INACTIVE)
    return;

  if (state_ != State::COMPLETE) {
    Stop(InstallableStatusCode::PIPELINE_RESTARTED);
  }

  UpdateState(State::INACTIVE);
  RequestAppBanner(validated_url_);
}

void AppBannerManager::TrackInstallPath(bool bottom_sheet,
                                        WebappInstallSource install_source) {
  install_path_tracker_.TrackInstallPath(bottom_sheet, install_source);
}

void AppBannerManager::TrackIphWasShown() {
  install_path_tracker_.TrackIphWasShown();
}

void AppBannerManager::Stop(InstallableStatusCode code) {
  ReportStatus(code);

  if (installable_web_app_check_result_ ==
      InstallableWebAppCheckResult::kUnknown) {
    SetInstallableWebAppCheckResult(InstallableWebAppCheckResult::kNo);
  }
  InvalidateWeakPtrsForThisNavigation();
  ResetBindings();
  UpdateState(State::COMPLETE);
  status_reporter_ = std::make_unique<NullStatusReporter>();
}

void AppBannerManager::SendBannerPromptRequest() {
  RecordCouldShowBanner();

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
                     GetWeakPtrForThisNavigation(), std::move(controller)));
}

void AppBannerManager::UpdateState(State state) {
  state_ = state;
}

void AppBannerManager::DidFinishNavigation(content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }

  if (state_ != State::COMPLETE && state_ != State::INACTIVE)
    Terminate();
  ResetCurrentPageData();

  if (handle->IsServedFromBackForwardCache()) {
    RequestAppBanner(validated_url_);
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
    RequestAppBanner(validated_url);
}

void AppBannerManager::DidActivatePortal(
    content::WebContents* predecessor_contents,
    base::TimeTicks activation_time) {
  // If this page was loaded in a portal, AppBannerManager may have been
  // instantiated after DidFinishLoad. Trigger the banner pipeline now (on
  // portal activation) if we missed the load event.
  if (!load_finished_ && !web_contents()->ShouldShowLoadingUI()) {
    DidFinishLoad(web_contents()->GetPrimaryMainFrame(),
                  web_contents()->GetLastCommittedURL());
  }
}

void AppBannerManager::DidUpdateWebManifestURL(
    content::RenderFrameHost* target_frame,
    const GURL& manifest_url) {
  GURL url = validated_url_;
  switch (state_) {
    case State::INACTIVE:
      return;
    case State::FETCHING_MANIFEST:
    case State::PENDING_INSTALLABLE_CHECK:
      UpdateState(State::INACTIVE);
      RequestAppBanner(validated_url_);
      return;
    case State::ACTIVE:
    case State::FETCHING_NATIVE_DATA:
    case State::PENDING_ENGAGEMENT:
    case State::SENDING_EVENT:
    case State::SENDING_EVENT_GOT_EARLY_PROMPT:
    case State::PENDING_PROMPT_CANCELED:
    case State::PENDING_PROMPT_NOT_CANCELED:
      Terminate();
      [[fallthrough]];
    case State::COMPLETE:
      if (!manifest_url.is_empty()) {
        RecheckInstallabilityForLoadedPage();
      }
      return;
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
  base::Erase(active_media_players_, id);
}

void AppBannerManager::WebContentsDestroyed() {
  Terminate();
}

void AppBannerManager::OnEngagementEvent(
    content::WebContents* contents,
    const GURL& url,
    double score,
    site_engagement::EngagementType /*type*/) {
  if (TriggeringDisabledForTesting()) {
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
    } else if (load_finished_ && state_ == State::INACTIVE) {
      // This performs some simple tests and starts async checks to test
      // installability. It should be safe to start in response to user input.
      // Don't call if we're already working on processing a banner request.
      RequestAppBanner(url);
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
      return manager->GetAppName();
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
      return manager->manifest_id_.spec();
  }
}
bool AppBannerManager::IsProbablyPromotableWebApp(
    bool ignore_existing_installations) const {
  bool in_promotable_scope =
      last_promotable_web_app_scope_.is_valid() &&
      base::StartsWith(web_contents()->GetLastCommittedURL().spec(),
                       last_promotable_web_app_scope_.spec(),
                       base::CompareCase::SENSITIVE);
  bool in_already_installed_scope =
      last_already_installed_web_app_scope_.is_valid() &&
      base::StartsWith(web_contents()->GetLastCommittedURL().spec(),
                       last_already_installed_web_app_scope_.spec(),
                       base::CompareCase::SENSITIVE);
  switch (installable_web_app_check_result_) {
    case InstallableWebAppCheckResult::kUnknown:
      return in_promotable_scope ||
             (ignore_existing_installations && in_already_installed_scope);
    case InstallableWebAppCheckResult::kNo:
    case InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      return ignore_existing_installations;
    case InstallableWebAppCheckResult::kYes_ByUserRequest:
      return false;
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

const GURL& AppBannerManager::GetManifestStartUrl() const {
  return manifest().start_url;
}

blink::mojom::DisplayMode AppBannerManager::GetManifestDisplayMode() const {
  return manifest().display;
}

bool AppBannerManager::MaybeConsumeInstallAnimation() {
  DCHECK(IsProbablyPromotableWebApp());
  if (!install_animation_pending_)
    return false;
  AppBannerSettingsHelper::RecordInstallTextAnimationShown(
      web_contents(), last_promotable_web_app_scope_);
  install_animation_pending_ = false;
  return true;
}

void AppBannerManager::RecordCouldShowBanner() {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  AppBannerSettingsHelper::RecordBannerEvent(
      contents, validated_url_, GetAppIdentifier(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, GetCurrentTime());
}

void AppBannerManager::OnBannerPromptReply(
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
      MaybeShowAmbientBadge();
      UpdateState(State::PENDING_PROMPT_NOT_CANCELED);
    } else {
      UpdateState(State::PENDING_PROMPT_CANCELED);
    }
    return;
  }

  DCHECK_EQ(State::SENDING_EVENT_GOT_EARLY_PROMPT, state_);

  ShowBanner();
}

void AppBannerManager::MaybeShowAmbientBadge() {}

void AppBannerManager::ShowBanner() {
  // The banner is only shown if the site explicitly requests it to be shown.
  DCHECK_NE(State::SENDING_EVENT, state_);

  content::WebContents* contents = web_contents();
  WebappInstallSource install_source;

  TrackBeforeInstallEventPrompt(state_);

  install_source =
      status_reporter_->GetInstallSource(contents, InstallTrigger::API);

  DCHECK(!manifest_url_.is_empty());
  DCHECK(!blink::IsEmptyManifest(manifest()));
  DCHECK(!primary_icon_url_.is_empty());
  DCHECK(!primary_icon_.drawsNothing());

  TrackBeforeInstallEvent(BEFORE_INSTALL_EVENT_COMPLETE);
  ShowBannerUi(install_source);
  UpdateState(State::COMPLETE);
}

void AppBannerManager::DisplayAppBanner() {
  // Prevent this from being called multiple times on the same connection.
  receiver_.reset();

  if (state_ == State::PENDING_PROMPT_CANCELED ||
      state_ == State::PENDING_PROMPT_NOT_CANCELED) {
    ShowBanner();
  } else if (state_ == State::SENDING_EVENT) {
    // Log that the prompt request was made for when we get the prompt reply.
    UpdateState(State::SENDING_EVENT_GOT_EARLY_PROMPT);
  }
}

}  // namespace webapps
