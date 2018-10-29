// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui_base.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/issue_manager.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/fullscreen_video_element.mojom.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "url/origin.h"

namespace media_router {
namespace {

std::string TruncateHost(const std::string& host) {
  const std::string truncated =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  // The truncation will be empty in some scenarios (e.g. host is
  // simply an IP address). Fail gracefully.
  return truncated.empty() ? host : truncated;
}

// Returns the first source in |sources| that can be connected to, or an empty
// source if there is none.  This is used by the Media Router to find such a
// matching route if it exists.
MediaSource GetSourceForRouteObserver(const std::vector<MediaSource>& sources) {
  auto source_it =
      std::find_if(sources.begin(), sources.end(), IsCastPresentationUrl);
  return source_it != sources.end() ? *source_it : MediaSource("");
}

}  // namespace

// Observes a WebContents and requests fullscreening of its first
// video element.  The request is sent after the WebContents is loaded and tab
// capture has begun. Marked final to prevent inheritance so delete calls are
// contained to scenarios documented below.
class MediaRouterUIBase::WebContentsFullscreenOnLoadedObserver final
    : public content::WebContentsObserver {
 public:
  WebContentsFullscreenOnLoadedObserver(const GURL& file_url,
                                        content::WebContents* web_contents)
      : file_url_(file_url) {
    DCHECK(file_url_.SchemeIsFile());
    DCHECK(fullscreen_request_time_.is_null());

    // If the WebContents is loading, start listening, otherwise just call the
    // fullscreen function.

    // This class destroys itself in the following situations (at least one of
    // which will occur):
    //   * after loading is complete and,
    //   ** capture has begun and fullscreen requested,
    //   ** kMaxSecondsToWaitForCapture seconds have passed without capture,
    //   * another navigation is started,
    //   * the WebContents is destroyed.
    if (web_contents->IsLoading()) {
      Observe(web_contents);
    } else {
      FullScreenFirstVideoElement(web_contents);
    }
  }
  ~WebContentsFullscreenOnLoadedObserver() override {}

  // content::WebContentsObserver implementation.
  void DidStopLoading() override {
    FullScreenFirstVideoElement(web_contents());
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    // If the user takes over and navigates away from the file, stop listening.
    // (It is possible however for this listener to be created before the
    // navigation to the requested file triggers, so provided we're still on the
    // same URL, go ahead and keep listening).
    if (file_url_ != navigation_handle->GetURL()) {
      delete this;
    }
  }

  void WebContentsDestroyed() override {
    // If the WebContents is destroyed we will never trigger and need to clean
    // up.
    delete this;
  }

 private:
  const GURL file_url_;

  // Time intervals used by the logic that detects if capture has started.
  const int kMaxSecondsToWaitForCapture = 10;
  const int kPollIntervalInSeconds = 1;

  // The time at which fullscreen was requested.
  base::TimeTicks fullscreen_request_time_;

  // Poll timer to monitor the capturer count when fullscreening local files.
  //
  // TODO(crbug.com/540965): Add a method to WebContentsObserver to report
  // capturer count changes and get rid of this polling-based approach.
  base::OneShotTimer capture_poll_timer_;

  // Sends a request for full screen to the WebContents targeted at the first
  // video element.  The request is only sent after capture has begun.
  void FullScreenFirstVideoElement(content::WebContents* web_contents) {
    if (file_url_ != web_contents->GetLastCommittedURL()) {
      // The user has navigated before the casting started. Do not attempt to
      // fullscreen and cleanup.
      return;
    }

    fullscreen_request_time_ = base::TimeTicks::Now();
    FullscreenIfContentCaptured(web_contents);
  }

  void FullscreenIfContentCaptured(content::WebContents* web_contents) {
    if (web_contents->IsBeingCaptured()) {
      content::mojom::FullscreenVideoElementHandlerAssociatedPtr client;
      web_contents->GetMainFrame()
          ->GetRemoteAssociatedInterfaces()
          ->GetInterface(&client);
      client->RequestFullscreenVideoElement();
      delete this;
      return;
    } else if (base::TimeTicks::Now() - fullscreen_request_time_ >
               base::TimeDelta::FromSeconds(kMaxSecondsToWaitForCapture)) {
      // If content capture hasn't started within the timeout skip fullscreen.
      DLOG(WARNING) << "Capture of local content did not start within timeout";
      delete this;
      return;
    }

    capture_poll_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kPollIntervalInSeconds),
        base::BindRepeating(
            &WebContentsFullscreenOnLoadedObserver::FullscreenIfContentCaptured,
            base::Unretained(this), web_contents));
  }
};

MediaRouterUIBase::MediaRouterUIBase()
    : initiator_(nullptr), weak_factory_(this) {}

MediaRouterUIBase::~MediaRouterUIBase() {
  if (query_result_manager_.get())
    query_result_manager_->RemoveObserver(this);
  if (presentation_service_delegate_.get())
    presentation_service_delegate_->RemoveDefaultPresentationRequestObserver(
        this);
  // If |start_presentation_context_| still exists, then it means presentation
  // route request was never attempted.
  if (start_presentation_context_) {
    bool presentation_sinks_available = std::any_of(
        sinks_.begin(), sinks_.end(), [](const MediaSinkWithCastModes& sink) {
          return base::ContainsKey(sink.cast_modes,
                                   MediaCastMode::PRESENTATION);
        });
    if (presentation_sinks_available) {
      start_presentation_context_->InvokeErrorCallback(
          blink::mojom::PresentationError(blink::mojom::PresentationErrorType::
                                              PRESENTATION_REQUEST_CANCELLED,
                                          "Dialog closed."));
    } else {
      start_presentation_context_->InvokeErrorCallback(
          blink::mojom::PresentationError(
              blink::mojom::PresentationErrorType::NO_AVAILABLE_SCREENS,
              "No screens found."));
    }
  }
}

void MediaRouterUIBase::InitWithDefaultMediaSource(
    content::WebContents* initiator,
    PresentationServiceDelegateImpl* delegate) {
  DCHECK(initiator);
  DCHECK(!presentation_service_delegate_);
  DCHECK(!query_result_manager_);

  InitCommon(initiator);
  if (delegate) {
    presentation_service_delegate_ = delegate->GetWeakPtr();
    presentation_service_delegate_->AddDefaultPresentationRequestObserver(this);
  }

  if (delegate && delegate->HasDefaultPresentationRequest()) {
    OnDefaultPresentationChanged(delegate->GetDefaultPresentationRequest());
  } else {
    // Register for MediaRoute updates without a media source.
    routes_observer_ = std::make_unique<UIMediaRoutesObserver>(
        GetMediaRouter(), MediaSource::Id(),
        base::BindRepeating(&MediaRouterUIBase::OnRoutesUpdated,
                            base::Unretained(this)));
  }
}

void MediaRouterUIBase::InitWithStartPresentationContext(
    content::WebContents* initiator,
    PresentationServiceDelegateImpl* delegate,
    std::unique_ptr<StartPresentationContext> context) {
  DCHECK(initiator);
  DCHECK(delegate);
  DCHECK(context);
  DCHECK(!start_presentation_context_);
  DCHECK(!query_result_manager_);

  start_presentation_context_ = std::move(context);
  presentation_service_delegate_ = delegate->GetWeakPtr();

  InitCommon(initiator);
  OnDefaultPresentationChanged(
      start_presentation_context_->presentation_request());
}

bool MediaRouterUIBase::CreateRoute(const MediaSink::Id& sink_id,
                                    MediaCastMode cast_mode) {
  // Default the tab casting the content to the initiator, and change if
  // necessary.
  content::WebContents* tab_contents = initiator_;

  base::Optional<RouteParameters> params;
  if (cast_mode == MediaCastMode::LOCAL_FILE) {
    GURL url = media_router_file_dialog_->GetLastSelectedFileUrl();
    tab_contents = OpenTabWithUrl(url);
    params = GetLocalFileRouteParameters(sink_id, url, tab_contents);
  } else {
    params = GetRouteParameters(sink_id, cast_mode);
  }
  if (!params) {
    SendIssueForUnableToCast(cast_mode);
    return false;
  }

  GetIssueManager()->ClearNonBlockingIssues();
  GetMediaRouter()->CreateRoute(
      params->source_id, sink_id, params->origin, tab_contents,
      base::BindOnce(&MediaRouterUIBase::RunRouteResponseCallbacks,
                     std::move(params->presentation_callback),
                     std::move(params->route_result_callbacks)),
      params->timeout, params->incognito);
  return true;
}

void MediaRouterUIBase::TerminateRoute(const MediaRoute::Id& route_id) {
  GetMediaRouter()->TerminateRoute(route_id);
}

void MediaRouterUIBase::MaybeReportCastingSource(
    MediaCastMode cast_mode,
    const RouteRequestResult& result) {
  if (result.result_code() == RouteRequestResult::OK)
    MediaRouterMetrics::RecordMediaRouterCastingSource(cast_mode);
}

std::vector<MediaSinkWithCastModes> MediaRouterUIBase::GetEnabledSinks() const {
  if (!display_observer_)
    return sinks_;

  // Filter out the wired display sink for the display that the dialog is on.
  // This is not the best place to do this because MRUI should not perform a
  // provider-specific behavior, but we currently do not have a way to
  // communicate dialog-specific information to/from the
  // WiredDisplayMediaRouteProvider.
  std::vector<MediaSinkWithCastModes> enabled_sinks;
  const std::string display_sink_id =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(
          display_observer_->GetCurrentDisplay());
  for (const MediaSinkWithCastModes& sink : sinks_) {
    if (sink.sink.id() != display_sink_id)
      enabled_sinks.push_back(sink);
  }
  return enabled_sinks;
}

std::string MediaRouterUIBase::GetTruncatedPresentationRequestSourceName()
    const {
  GURL gurl = GetFrameURL();
  CHECK(initiator_);
  return gurl.SchemeIs(extensions::kExtensionScheme)
             ? GetExtensionName(gurl, extensions::ExtensionRegistry::Get(
                                          initiator_->GetBrowserContext()))
             : TruncateHost(GetHostFromURL(gurl));
}

void MediaRouterUIBase::AddIssue(const IssueInfo& issue) {
  GetIssueManager()->AddIssue(issue);
}

void MediaRouterUIBase::RemoveIssue(const Issue::Id& issue_id) {
  GetIssueManager()->ClearIssue(issue_id);
}

void MediaRouterUIBase::OpenFileDialog() {
  if (!media_router_file_dialog_) {
    media_router_file_dialog_ = std::make_unique<MediaRouterFileDialog>(this);
  }

  media_router_file_dialog_->OpenFileDialog(GetBrowser());
}

MediaRouterUIBase::RouteRequest::RouteRequest(const MediaSink::Id& sink_id)
    : sink_id(sink_id) {
  static base::AtomicSequenceNumber g_next_request_id;
  id = g_next_request_id.GetNext();
}

MediaRouterUIBase::RouteRequest::~RouteRequest() = default;

// static
void MediaRouterUIBase::RunRouteResponseCallbacks(
    MediaRouteResponseCallback presentation_callback,
    std::vector<MediaRouteResultCallback> callbacks,
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (presentation_callback)
    std::move(presentation_callback).Run(std::move(connection), result);
  DCHECK(!connection);
  for (auto& callback : callbacks)
    std::move(callback).Run(result);
}

std::vector<MediaSource> MediaRouterUIBase::GetSourcesForCastMode(
    MediaCastMode cast_mode) const {
  return query_result_manager_->GetSourcesForCastMode(cast_mode);
}

void MediaRouterUIBase::OnResultsUpdated(
    const std::vector<MediaSinkWithCastModes>& sinks) {
  sinks_ = sinks;

  const icu::Collator* collator_ptr = collator_.get();
  std::sort(sinks_.begin(), sinks_.end(),
            [collator_ptr](const MediaSinkWithCastModes& sink1,
                           const MediaSinkWithCastModes& sink2) {
              return sink1.sink.CompareUsingCollator(sink2.sink, collator_ptr);
            });
  UpdateSinks();
}

void MediaRouterUIBase::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids) {
  routes_.clear();

  for (const MediaRoute& route : routes) {
    if (route.for_display()) {
#ifndef NDEBUG
      for (const MediaRoute& existing_route : routes_) {
        if (existing_route.media_sink_id() == route.media_sink_id()) {
          DVLOG(2) << "Received another route for display with the same sink"
                   << " id as an existing route. " << route.media_route_id()
                   << " has the same sink id as "
                   << existing_route.media_sink_id() << ".";
        }
      }
#endif
      routes_.push_back(route);
    }
  }
}

void MediaRouterUIBase::OnRouteResponseReceived(
    int route_request_id,
    const MediaSink::Id& sink_id,
    MediaCastMode cast_mode,
    const base::string16& presentation_request_source_name,
    const RouteRequestResult& result) {
  DVLOG(1) << "OnRouteResponseReceived";
  // If we receive a new route that we aren't expecting, do nothing.
  if (!current_route_request_ || route_request_id != current_route_request_->id)
    return;

  const MediaRoute* route = result.route();
  if (!route) {
    // The provider will handle sending an issue for a failed route request.
    DVLOG(1) << "MediaRouteResponse returned error: " << result.error();
  }

  current_route_request_.reset();
}

void MediaRouterUIBase::HandleCreateSessionRequestRouteResponse(
    const RouteRequestResult&) {}

void MediaRouterUIBase::InitCommon(content::WebContents* initiator) {
  DCHECK(initiator);
  initiator_ = initiator;

  GetMediaRouter()->OnUserGesture();

  // Create |collator_| before |query_result_manager_| so that |collator_| is
  // already set up when we get a callback from |query_result_manager_|.
  UErrorCode error = U_ZERO_ERROR;
  const std::string& locale = g_browser_process->GetApplicationLocale();
  collator_.reset(
      icu::Collator::createInstance(icu::Locale(locale.c_str()), error));
  if (U_FAILURE(error)) {
    DLOG(ERROR) << "Failed to create collator for locale " << locale;
    collator_.reset();
  }

  query_result_manager_ =
      std::make_unique<QueryResultManager>(GetMediaRouter());
  query_result_manager_->AddObserver(this);

  // Use a placeholder URL as origin for mirroring.
  url::Origin origin = url::Origin::Create(GURL());

  // Desktop mirror mode is always available.
  query_result_manager_->SetSourcesForCastMode(
      MediaCastMode::DESKTOP_MIRROR, {MediaSourceForDesktop()}, origin);

  // File mirroring is always availible.
  query_result_manager_->SetSourcesForCastMode(MediaCastMode::LOCAL_FILE,
                                               {MediaSourceForTab(0)}, origin);

  SessionID::id_type tab_id = SessionTabHelper::IdForTab(initiator).id();
  if (tab_id != -1) {
    MediaSource mirroring_source(MediaSourceForTab(tab_id));
    query_result_manager_->SetSourcesForCastMode(MediaCastMode::TAB_MIRROR,
                                                 {mirroring_source}, origin);
  }

  // Get the current list of media routes, so that the WebUI will have routes
  // information at initialization.
  OnRoutesUpdated(GetMediaRouter()->GetCurrentRoutes(),
                  std::vector<MediaRoute::Id>());
  display_observer_ = WebContentsDisplayObserver::Create(
      initiator_, base::BindRepeating(&MediaRouterUIBase::UpdateSinks,
                                      base::Unretained(this)));
}

void MediaRouterUIBase::OnDefaultPresentationChanged(
    const content::PresentationRequest& presentation_request) {
  std::vector<MediaSource> sources =
      MediaSourcesForPresentationUrls(presentation_request.presentation_urls);
  presentation_request_ = presentation_request;
  query_result_manager_->SetSourcesForCastMode(
      MediaCastMode::PRESENTATION, sources,
      presentation_request_->frame_origin);
  // Register for MediaRoute updates.  NOTE(mfoltz): If there are multiple
  // sources that can be connected to via the dialog, this will break.  We will
  // need to observe multiple sources (keyed by sinks) in that case.  As this is
  // Cast-specific for the forseeable future, it may be simpler to plumb a new
  // observer API for this case.
  const MediaSource source_for_route_observer =
      GetSourceForRouteObserver(sources);
  routes_observer_ = std::make_unique<UIMediaRoutesObserver>(
      GetMediaRouter(), source_for_route_observer.id(),
      base::BindRepeating(&MediaRouterUIBase::OnRoutesUpdated,
                          base::Unretained(this)));
}

void MediaRouterUIBase::OnDefaultPresentationRemoved() {
  presentation_request_.reset();
  query_result_manager_->RemoveSourcesForCastMode(MediaCastMode::PRESENTATION);

  // Register for MediaRoute updates without a media source.
  routes_observer_ = std::make_unique<UIMediaRoutesObserver>(
      GetMediaRouter(), MediaSource::Id(),
      base::BindRepeating(&MediaRouterUIBase::OnRoutesUpdated,
                          base::Unretained(this)));
}

base::Optional<RouteParameters> MediaRouterUIBase::GetRouteParameters(
    const MediaSink::Id& sink_id,
    MediaCastMode cast_mode) {
  DCHECK(query_result_manager_);
  DCHECK(initiator_);

  RouteParameters params;

  // Note that there is a rarely-encountered bug, where the MediaCastMode to
  // MediaSource mapping could have been updated, between when the user clicked
  // on the UI to start a create route request, and when this function is
  // called. However, since the user does not have visibility into the
  // MediaSource, and that it occurs very rarely in practice, we leave it as-is
  // for now.
  std::unique_ptr<MediaSource> source =
      query_result_manager_->GetSourceForCastModeAndSink(cast_mode, sink_id);

  if (!source) {
    LOG(ERROR) << "No corresponding MediaSource for cast mode "
               << static_cast<int>(cast_mode) << " and sink " << sink_id;
    return base::nullopt;
  }
  params.source_id = source->id();

  bool for_presentation_source = cast_mode == MediaCastMode::PRESENTATION;
  if (for_presentation_source && !presentation_request_) {
    DLOG(ERROR) << "Requested to create a route for presentation, but "
                << "presentation request is missing.";
    return base::nullopt;
  }

  current_route_request_ = base::make_optional<RouteRequest>(sink_id);
  params.origin = for_presentation_source ? presentation_request_->frame_origin
                                          : url::Origin::Create(GURL());
  DVLOG(1) << "DoCreateRoute: origin: " << params.origin;

  // This callback must be invoked before
  // HandleCreateSessionRequestRouteResponse(), which closes the dialog and
  // destroys |this|.
  params.route_result_callbacks.push_back(
      base::BindOnce(&MediaRouterUIBase::MaybeReportCastingSource,
                     weak_factory_.GetWeakPtr(), cast_mode));

  // There are 3 cases. In cases (1) and (3) the MediaRouterUIBase will need to
  // be notified via OnRouteResponseReceived(). In case (2) the dialog will be
  // closed before that via HandleCreateSessionRequestRouteResponse().
  // (1) Non-presentation route request (e.g., mirroring). No additional
  //     notification necessary.
  // (2) Presentation route request for a PresentationRequest.start() call.
  //     The StartPresentationContext will need to be answered with the route
  //     response.
  // (3) Browser-initiated presentation route request. If successful,
  //     PresentationServiceDelegateImpl will have to be notified. Note that we
  //     treat subsequent route requests from a Presentation API-initiated
  //     dialogs as browser-initiated.
  // TODO(https://crbug.com/868186): Close the Views dialog in case (2).
  params.route_result_callbacks.push_back(base::BindOnce(
      &MediaRouterUIBase::OnRouteResponseReceived, weak_factory_.GetWeakPtr(),
      current_route_request_->id, sink_id, cast_mode,
      base::UTF8ToUTF16(GetTruncatedPresentationRequestSourceName())));
  if (for_presentation_source) {
    if (start_presentation_context_) {
      // |start_presentation_context_| will be nullptr after this call, as the
      // object will be transferred to the callback.
      params.presentation_callback =
          base::BindOnce(&StartPresentationContext::HandleRouteResponse,
                         std::move(start_presentation_context_));
      params.route_result_callbacks.push_back(base::BindOnce(
          &MediaRouterUIBase::HandleCreateSessionRequestRouteResponse,
          weak_factory_.GetWeakPtr()));
    } else if (presentation_service_delegate_) {
      params.presentation_callback = base::BindOnce(
          &PresentationServiceDelegateImpl::OnRouteResponse,
          presentation_service_delegate_, *presentation_request_);
    }
  }

  params.timeout = GetRouteRequestTimeout(cast_mode);
  CHECK(initiator_);
  params.incognito = initiator_->GetBrowserContext()->IsOffTheRecord();

  return base::make_optional(std::move(params));
}

GURL MediaRouterUIBase::GetFrameURL() const {
  return presentation_request_ ? presentation_request_->frame_origin.GetURL()
                               : GURL();
}

void MediaRouterUIBase::SendIssueForRouteTimeout(
    MediaCastMode cast_mode,
    const base::string16& presentation_request_source_name) {
  std::string issue_title;
  switch (cast_mode) {
    case PRESENTATION:
      DLOG_IF(ERROR, presentation_request_source_name.empty())
          << "Empty presentation request source name.";
      issue_title =
          l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT,
                                    presentation_request_source_name);
      break;
    case TAB_MIRROR:
      issue_title = l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
      break;
    case DESKTOP_MIRROR:
      issue_title = l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_DESKTOP);
      break;
    case LOCAL_FILE:
      issue_title = l10n_util::GetStringUTF8(
          IDS_MEDIA_ROUTER_ISSUE_FILE_CAST_GENERIC_ERROR);
      break;
  }

  AddIssue(IssueInfo(issue_title, IssueInfo::Action::DISMISS,
                     IssueInfo::Severity::NOTIFICATION));
}

void MediaRouterUIBase::SendIssueForUnableToCast(MediaCastMode cast_mode) {
  // For a generic error, claim a tab error unless it was specifically desktop
  // mirroring.
  std::string issue_title =
      (cast_mode == MediaCastMode::DESKTOP_MIRROR)
          ? l10n_util::GetStringUTF8(
                IDS_MEDIA_ROUTER_ISSUE_UNABLE_TO_CAST_DESKTOP)
          : l10n_util::GetStringUTF8(
                IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
  AddIssue(IssueInfo(issue_title, IssueInfo::Action::DISMISS,
                     IssueInfo::Severity::WARNING));
}

IssueManager* MediaRouterUIBase::GetIssueManager() {
  return GetMediaRouter()->GetIssueManager();
}

void MediaRouterUIBase::StartObservingIssues() {
  issues_observer_ =
      std::make_unique<UiIssuesObserver>(GetIssueManager(), this);
  issues_observer_->Init();
}

void MediaRouterUIBase::FileDialogSelectionFailed(const IssueInfo& issue) {
  AddIssue(issue);
}

MediaRouterUIBase::UiIssuesObserver::UiIssuesObserver(
    IssueManager* issue_manager,
    MediaRouterUIBase* ui)
    : IssuesObserver(issue_manager), ui_(ui) {
  DCHECK(ui);
}

MediaRouterUIBase::UiIssuesObserver::~UiIssuesObserver() = default;

void MediaRouterUIBase::UiIssuesObserver::OnIssue(const Issue& issue) {
  ui_->OnIssue(issue);
}

void MediaRouterUIBase::UiIssuesObserver::OnIssuesCleared() {
  ui_->OnIssueCleared();
}

MediaRouterUIBase::UIMediaRoutesObserver::UIMediaRoutesObserver(
    MediaRouter* router,
    const MediaSource::Id& source_id,
    const RoutesUpdatedCallback& callback)
    : MediaRoutesObserver(router, source_id), callback_(callback) {
  DCHECK(!callback_.is_null());
}

MediaRouterUIBase::UIMediaRoutesObserver::~UIMediaRoutesObserver() = default;

void MediaRouterUIBase::UIMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids) {
  callback_.Run(routes, joinable_route_ids);
}

base::Optional<RouteParameters> MediaRouterUIBase::GetLocalFileRouteParameters(
    const MediaSink::Id& sink_id,
    const GURL& file_url,
    content::WebContents* tab_contents) {
  RouteParameters params;
  SessionID::id_type tab_id = SessionTabHelper::IdForTab(tab_contents).id();
  params.source_id = MediaSourceForTab(tab_id).id();

  // Use a placeholder URL as origin for local file casting, which is
  // essentially mirroring.
  params.origin = url::Origin::Create(GURL(chrome::kChromeUIMediaRouterURL));

  int request_id = current_route_request() ? current_route_request()->id : -1;
  params.route_result_callbacks.push_back(base::BindOnce(
      &MediaRouterUIBase::OnRouteResponseReceived, weak_factory_.GetWeakPtr(),
      request_id, sink_id, MediaCastMode::LOCAL_FILE,
      base::UTF8ToUTF16(GetTruncatedPresentationRequestSourceName())));

  params.route_result_callbacks.push_back(
      base::BindOnce(&MediaRouterUIBase::MaybeReportCastingSource,
                     weak_factory_.GetWeakPtr(), MediaCastMode::LOCAL_FILE));

  params.route_result_callbacks.push_back(
      base::BindOnce(&MediaRouterUIBase::MaybeReportFileInformation,
                     weak_factory_.GetWeakPtr()));

  params.route_result_callbacks.push_back(
      base::BindOnce(&MediaRouterUIBase::FullScreenFirstVideoElement,
                     weak_factory_.GetWeakPtr(), file_url, tab_contents));

  params.timeout = GetRouteRequestTimeout(MediaCastMode::LOCAL_FILE);
  CHECK(initiator_);
  params.incognito = initiator_->GetBrowserContext()->IsOffTheRecord();

  return base::make_optional(std::move(params));
}

// TODO(crbug.com/792547): Refactor FullScreenFirstVideoElement() and
// MaybeReportFileInformation() into a local media casting specific location
// instead of here in the main ui.
void MediaRouterUIBase::FullScreenFirstVideoElement(
    const GURL& file_url,
    content::WebContents* web_contents,
    const RouteRequestResult& result) {
  if (result.result_code() == RouteRequestResult::OK) {
    new WebContentsFullscreenOnLoadedObserver(file_url, web_contents);
  }
}

void MediaRouterUIBase::MaybeReportFileInformation(
    const RouteRequestResult& result) {
  if (result.result_code() == RouteRequestResult::OK)
    media_router_file_dialog_->MaybeReportLastSelectedFileInformation();
}

content::WebContents* MediaRouterUIBase::OpenTabWithUrl(const GURL& url) {
  // Check if the current page is a new tab. If so open file in current page.
  // If not then open a new page.
  if (initiator_->GetVisibleURL() == chrome::kChromeUINewTabURL) {
    content::NavigationController::LoadURLParams load_params(url);
    load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
    initiator_->GetController().LoadURLWithParams(load_params);
    return initiator_;
  } else {
    return chrome::AddSelectedTabWithURL(GetBrowser(), url,
                                         ui::PAGE_TRANSITION_LINK);
  }
}

MediaRouter* MediaRouterUIBase::GetMediaRouter() const {
  CHECK(initiator_);
  return MediaRouterFactory::GetApiForBrowserContext(
      initiator_->GetBrowserContext());
}

Browser* MediaRouterUIBase::GetBrowser() {
  CHECK(initiator_);
  return chrome::FindBrowserWithWebContents(initiator_);
}

}  // namespace media_router
