// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/issue_manager.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/route_request_result.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/media/fullscreen_video_element.mojom.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "ui/base/cocoa/permissions_utils.h"
#endif

namespace media_router {

namespace {

constexpr char kLoggerComponent[] = "MediaRouterUI";

// Returns true if |issue| is associated with |ui_sink|.
bool IssueMatches(const Issue& issue, const UIMediaSink& ui_sink) {
  return issue.info().sink_id == ui_sink.id ||
         (!issue.info().route_id.empty() && ui_sink.route &&
          issue.info().route_id == ui_sink.route->media_route_id());
}

std::u16string GetSinkFriendlyName(const MediaSink& sink) {
  // Use U+2010 (HYPHEN) instead of ASCII hyphen to avoid problems with RTL
  // languages.
  const char* separator = u8" \u2010 ";
  return base::UTF8ToUTF16(sink.description() ? sink.name() + separator +
                                                    sink.description().value()
                                              : sink.name());
}

void MaybeReportCastingSource(MediaCastMode cast_mode,
                              const RouteRequestResult& result) {
  if (result.result_code() == RouteRequestResult::OK)
    base::UmaHistogramSparse("MediaRouter.Source.CastingSource", cast_mode);
}

void RunRouteResponseCallbacks(
    MediaRouteResponseCallback presentation_callback,
    std::vector<MediaRouteResultCallback> route_result_callbacks,
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  if (presentation_callback)
    std::move(presentation_callback).Run(std::move(connection), result);
  DCHECK(!connection);
  for (auto& callback : route_result_callbacks)
    std::move(callback).Run(result);
}

}  // namespace

MediaRouterUI::MediaRouterUI(content::WebContents* initiator)
    : presentation_manager_(WebContentsPresentationManager::Get(initiator)),
      initiator_(initiator),
      logger_(GetMediaRouter()->GetLogger()) {
  CHECK(initiator_);
  if (presentation_manager_)
    presentation_manager_->AddObserver(this);
}

MediaRouterUI::~MediaRouterUI() {
  for (CastDialogController::Observer& observer : observers_)
    observer.OnControllerInvalidated();

  if (query_result_manager_.get())
    query_result_manager_->RemoveObserver(this);
  if (presentation_manager_)
    presentation_manager_->RemoveObserver(this);

  // If |start_presentation_context_| still exists, then it means presentation
  // route request was never attempted.
  if (start_presentation_context_) {
    std::vector<MediaSinkWithCastModes> sinks;
    if (query_result_manager_.get()) {
      sinks = query_result_manager_->GetSinksWithCastModes();
    }
    bool presentation_sinks_available = std::any_of(
        sinks.begin(), sinks.end(), [](const MediaSinkWithCastModes& sink) {
          return base::Contains(sink.cast_modes, MediaCastMode::PRESENTATION);
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

void MediaRouterUI::AddObserver(CastDialogController::Observer* observer) {
  observers_.AddObserver(observer);
  // TODO(takumif): Update the header when this object is initialized instead.
  UpdateModelHeader();
}

void MediaRouterUI::RemoveObserver(CastDialogController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MediaRouterUI::StartCasting(const std::string& sink_id,
                                 MediaCastMode cast_mode) {
  CreateRoute(sink_id, cast_mode);
}

void MediaRouterUI::StopCasting(const std::string& route_id) {
  terminating_route_id_ = route_id;
  // |route_id| may become invalid after UpdateSinks(), so we cannot refer to
  // |route_id| below this line.
  UpdateSinks();
  TerminateRoute(terminating_route_id_.value());
}

void MediaRouterUI::ClearIssue(const Issue::Id& issue_id) {
  RemoveIssue(issue_id);
}

content::WebContents* MediaRouterUI::GetInitiator() {
  return initiator();
}

std::unique_ptr<StartPresentationContext>
MediaRouterUI::TakeStartPresentationContext() {
  return std::move(start_presentation_context_);
}

void MediaRouterUI::InitWithDefaultMediaSource() {
  DCHECK(!query_result_manager_);
  InitCommon();

  if (presentation_manager_ &&
      presentation_manager_->HasDefaultPresentationRequest()) {
    OnDefaultPresentationChanged(
        &presentation_manager_->GetDefaultPresentationRequest());
  }
}

void MediaRouterUI::InitWithDefaultMediaSourceAndMirroring() {
  InitWithDefaultMediaSource();
  InitMirroring();
}

void MediaRouterUI::InitWithStartPresentationContext(
    std::unique_ptr<StartPresentationContext> context) {
  DCHECK(context);
  DCHECK(!start_presentation_context_);
  DCHECK(!query_result_manager_);

  start_presentation_context_ = std::move(context);

  InitCommon();
  OnDefaultPresentationChanged(
      &start_presentation_context_->presentation_request());
}

void MediaRouterUI::InitWithStartPresentationContextAndMirroring(
    std::unique_ptr<StartPresentationContext> context) {
  InitWithStartPresentationContext(std::move(context));
  InitMirroring();
}

bool MediaRouterUI::CreateRoute(const MediaSink::Id& sink_id,
                                MediaCastMode cast_mode) {
  logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                   "CreateRoute requested by MediaRouterViewsUI.", sink_id, "",
                   "");
  if (RequiresScreenCapturePermission(cast_mode)) {
    const bool screen_capture_allowed = GetScreenCapturePermission();
    if (!screen_capture_allowed) {
      SendIssueForScreenPermission(sink_id);
      return false;
    }
  }

  // Default the tab casting the content to the initiator, and change if
  // necessary.
  content::WebContents* tab_contents = initiator_;

  absl::optional<RouteParameters> params =
      GetRouteParameters(sink_id, cast_mode);
  if (!params) {
    SendIssueForUnableToCast(cast_mode, sink_id);
    return false;
  }
  GetIssueManager()->ClearNonBlockingIssues();
  GetMediaRouter()->CreateRoute(
      params->source_id, sink_id, params->origin, tab_contents,
      base::BindOnce(&RunRouteResponseCallbacks,
                     std::move(params->presentation_callback),
                     std::move(params->route_result_callbacks)),
      params->timeout, params->off_the_record);

  // TODO(crbug.com/1015203): This call to UpdateSinks() was originally in
  // StartCasting(), but it causes Chrome to crash when the desktop picker
  // dialog is shown, so for now we just don't call it in that case.  Move it
  // back once the problem is resolved.
  if (cast_mode != MediaCastMode::DESKTOP_MIRROR)
    UpdateSinks();

  return true;
}

void MediaRouterUI::TerminateRoute(const MediaRoute::Id& route_id) {
  logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                   "TerminateRoute requested by MediaRouterUI.",
                   MediaRoute::GetSinkIdFromMediaRouteId(route_id),
                   MediaRoute::GetMediaSourceIdFromMediaRouteId(route_id),
                   MediaRoute::GetPresentationIdFromMediaRouteId(route_id));
  GetMediaRouter()->TerminateRoute(route_id);
}

std::vector<MediaSinkWithCastModes> MediaRouterUI::GetEnabledSinks() const {
  if (!display_observer_)
    return sinks_;

  // Filter out the wired display sink for the display that the dialog is on.
  // This is not the best place to do this because MRUI should not perform a
  // provider-specific behavior, but we currently do not have a way to
  // communicate dialog-specific information to/from the
  // WiredDisplayMediaRouteProvider.
  std::vector<MediaSinkWithCastModes> enabled_sinks(sinks_);
  const std::string display_sink_id =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(
          display_observer_->GetCurrentDisplay());
  base::EraseIf(enabled_sinks,
                [&display_sink_id](const MediaSinkWithCastModes& sink) {
                  return sink.sink.id() == display_sink_id;
                });

  return enabled_sinks;
}

std::u16string MediaRouterUI::GetPresentationRequestSourceName() const {
  const url::Origin frame_origin = GetFrameOrigin();
  // Presentation URLs are only possible on https: and other secure contexts,
  // so we can omit http/https schemes here.
  return frame_origin.scheme() == extensions::kExtensionScheme
             ? base::UTF8ToUTF16(GetExtensionName(
                   frame_origin.GetURL(), extensions::ExtensionRegistry::Get(
                                              initiator_->GetBrowserContext())))
             : url_formatter::FormatOriginForSecurityDisplay(
                   frame_origin,
                   url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

void MediaRouterUI::AddIssue(const IssueInfo& issue) {
  GetIssueManager()->AddIssue(issue);
  switch (issue.severity) {
    case IssueInfo::Severity::NOTIFICATION:
      logger_->LogInfo(
          mojom::LogCategory::kUi, kLoggerComponent,
          base::StrCat({"Sink button shows an issue in NOTIFICATION level: ",
                        issue.title}),
          issue.sink_id,
          MediaRoute::GetMediaSourceIdFromMediaRouteId(issue.route_id),
          MediaRoute::GetPresentationIdFromMediaRouteId(issue.route_id));
      break;
    default:
      logger_->LogError(
          mojom::LogCategory::kUi, kLoggerComponent,
          base::StrCat(
              {"Sink button shows an issue in WARNING or FATAL level: ",
               issue.title}),
          issue.sink_id,
          MediaRoute::GetMediaSourceIdFromMediaRouteId(issue.route_id),
          MediaRoute::GetPresentationIdFromMediaRouteId(issue.route_id));
      break;
  }
}

void MediaRouterUI::RemoveIssue(const Issue::Id& issue_id) {
  GetIssueManager()->ClearIssue(issue_id);
}

void MediaRouterUI::LogMediaSinkStatus() {
  std::vector<std::string> sink_ids;
  for (const auto& sink : GetEnabledSinks()) {
    if (sink.sink.id().length() <= 4) {
      sink_ids.push_back(sink.sink.id());
    } else {
      sink_ids.push_back(sink.sink.id().substr(sink.sink.id().length() - 4));
    }
  }
  logger_->LogInfo(
      mojom::LogCategory::kUi, kLoggerComponent,
      base::StrCat(
          {base::StringPrintf("%zu sinks shown on CastDialogView closed: ",
                              sink_ids.size()),
           base::JoinString(sink_ids, ",")}),
      "", "", "");
}

MediaRouterUI::RouteRequest::RouteRequest(const MediaSink::Id& sink_id)
    : sink_id(sink_id) {
  static base::AtomicSequenceNumber g_next_request_id;
  id = g_next_request_id.GetNext();
}

MediaRouterUI::RouteRequest::~RouteRequest() = default;

MediaRouterUI::UiIssuesObserver::UiIssuesObserver(IssueManager* issue_manager,
                                                  MediaRouterUI* ui)
    : IssuesObserver(issue_manager), ui_(ui) {
  DCHECK(ui);
}

MediaRouterUI::UiIssuesObserver::~UiIssuesObserver() = default;

void MediaRouterUI::UiIssuesObserver::OnIssue(const Issue& issue) {
  ui_->OnIssue(issue);
}

void MediaRouterUI::UiIssuesObserver::OnIssuesCleared() {
  ui_->OnIssueCleared();
}

MediaRouterUI::UIMediaRoutesObserver::UIMediaRoutesObserver(
    MediaRouter* router,
    const RoutesUpdatedCallback& callback)
    : MediaRoutesObserver(router), callback_(callback) {
  DCHECK(!callback_.is_null());
}

MediaRouterUI::UIMediaRoutesObserver::~UIMediaRoutesObserver() = default;

void MediaRouterUI::UIMediaRoutesObserver::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes) {
  callback_.Run(routes);
}

std::vector<MediaSource> MediaRouterUI::GetSourcesForCastMode(
    MediaCastMode cast_mode) const {
  return query_result_manager_->GetSourcesForCastMode(cast_mode);
}

void MediaRouterUI::HandleCreateSessionRequestRouteResponse(
    const RouteRequestResult&) {
  // TODO(crbug.com/868186): Close the dialog.
}

void MediaRouterUI::InitCommon() {
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

  // Get the current list of media routes, so that the WebUI will have routes
  // information at initialization.
  OnRoutesUpdated(GetMediaRouter()->GetCurrentRoutes());
  display_observer_ = WebContentsDisplayObserver::Create(
      initiator_,
      base::BindRepeating(&MediaRouterUI::UpdateSinks, base::Unretained(this)));

  routes_observer_ = std::make_unique<UIMediaRoutesObserver>(
      GetMediaRouter(), base::BindRepeating(&MediaRouterUI::OnRoutesUpdated,
                                            base::Unretained(this)));

  StartObservingIssues();
}

void MediaRouterUI::InitMirroring() {
  // Use a placeholder URL as origin for mirroring.
  url::Origin origin = url::Origin::Create(GURL());

  // Desktop mirror mode is always available.
  query_result_manager_->SetSourcesForCastMode(
      MediaCastMode::DESKTOP_MIRROR, {MediaSource::ForUnchosenDesktop()},
      origin);

  SessionID::id_type tab_id =
      sessions::SessionTabHelper::IdForTab(initiator_).id();
  if (tab_id != -1) {
    MediaSource mirroring_source(MediaSource::ForTab(tab_id));
    query_result_manager_->SetSourcesForCastMode(MediaCastMode::TAB_MIRROR,
                                                 {mirroring_source}, origin);
  }
}

void MediaRouterUI::OnDefaultPresentationChanged(
    const content::PresentationRequest* presentation_request) {
  if (!presentation_request) {
    OnDefaultPresentationRemoved();
    return;
  }

  std::vector<MediaSource> sources;
  for (const auto& url : presentation_request->presentation_urls) {
    sources.push_back(MediaSource::ForPresentationUrl(url));
  }
  presentation_request_ = *presentation_request;
  query_result_manager_->SetSourcesForCastMode(
      MediaCastMode::PRESENTATION, sources,
      presentation_request_->frame_origin);
  UpdateModelHeader();
}

void MediaRouterUI::OnDefaultPresentationRemoved() {
  presentation_request_.reset();
  query_result_manager_->RemoveSourcesForCastMode(MediaCastMode::PRESENTATION);

  UpdateModelHeader();
}

void MediaRouterUI::UpdateSinks() {
  std::vector<UIMediaSink> media_sinks;
  for (const MediaSinkWithCastModes& sink : GetEnabledSinks()) {
    auto pred = [&sink](const MediaRoute& route) {
      return route.media_sink_id() == sink.sink.id();
    };
    auto route_it = std::find_if(routes().begin(), routes().end(), pred);
    const MediaRoute* route = route_it == routes().end() ? nullptr : &*route_it;
    media_sinks.push_back(ConvertToUISink(sink, route, issue_));
  }
  model_.set_media_sinks(std::move(media_sinks));
  for (CastDialogController::Observer& observer : observers_)
    observer.OnModelUpdated(model_);
}

absl::optional<RouteParameters> MediaRouterUI::GetRouteParameters(
    const MediaSink::Id& sink_id,
    MediaCastMode cast_mode) {
  DCHECK(query_result_manager_);
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
    logger_->LogError(
        mojom::LogCategory::kUi, kLoggerComponent,
        base::StringPrintf("No corresponding MediaSource for cast mode %d.",
                           static_cast<int>(cast_mode)),
        sink_id, "", "");
    return absl::nullopt;
  }
  params.source_id = source->id();

  bool for_presentation_source = cast_mode == MediaCastMode::PRESENTATION;
  if (for_presentation_source && !presentation_request_) {
    logger_->LogError(mojom::LogCategory::kUi, kLoggerComponent,
                      "Requested to create a route for presentation, but "
                      "presentation request is missing.",
                      sink_id, source->id(), "");
    return absl::nullopt;
  }

  current_route_request_ = absl::make_optional<RouteRequest>(sink_id);
  params.origin = for_presentation_source ? presentation_request_->frame_origin
                                          : url::Origin::Create(GURL());

  // This callback must be invoked before
  // HandleCreateSessionRequestRouteResponse(), which closes the dialog and
  // destroys |this|.
  params.route_result_callbacks.push_back(
      base::BindOnce(&MaybeReportCastingSource, cast_mode));

  // There are 3 cases. In cases (1) and (3) the MediaRouterViewsUI will need to
  // be notified via OnRouteResponseReceived(). In case (2) the dialog will be
  // closed before that via HandleCreateSessionRequestRouteResponse().
  // (1) Non-presentation route request (e.g., mirroring). No additional
  //     notification necessary.
  // (2) Presentation route request for a PresentationRequest.start() call.
  //     The StartPresentationContext will need to be answered with the route
  //     response.
  // (3) Browser-initiated presentation route request. If successful,
  //     WebContentsPresentationManager will have to be notified. Note that we
  //     treat subsequent route requests from a Presentation API-initiated
  //     dialogs as browser-initiated.
  // TODO(https://crbug.com/868186): Close the Views dialog in case (2).
  params.route_result_callbacks.push_back(
      base::BindOnce(&MediaRouterUI::OnRouteResponseReceived,
                     weak_factory_.GetWeakPtr(), current_route_request_->id,
                     sink_id, cast_mode, GetPresentationRequestSourceName()));
  if (for_presentation_source) {
    if (start_presentation_context_) {
      params.presentation_callback =
          base::BindOnce(&StartPresentationContext::HandleRouteResponse,
                         std::move(start_presentation_context_));
      params.route_result_callbacks.push_back(base::BindOnce(
          &MediaRouterUI::HandleCreateSessionRequestRouteResponse,
          weak_factory_.GetWeakPtr()));
    } else if (presentation_manager_) {
      params.presentation_callback = base::BindOnce(
          &WebContentsPresentationManager::OnPresentationResponse,
          presentation_manager_, *presentation_request_);
    }
  }

  params.timeout = GetRouteRequestTimeout(cast_mode);
  params.off_the_record = initiator_->GetBrowserContext()->IsOffTheRecord();

  return absl::make_optional(std::move(params));
}

url::Origin MediaRouterUI::GetFrameOrigin() const {
  return presentation_request_ ? presentation_request_->frame_origin
                               : url::Origin();
}

void MediaRouterUI::SendIssueForRouteTimeout(
    MediaCastMode cast_mode,
    const MediaSink::Id& sink_id,
    const std::u16string& presentation_request_source_name) {
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
  }

  IssueInfo issue_info(issue_title, IssueInfo::Action::DISMISS,
                       IssueInfo::Severity::NOTIFICATION);
  issue_info.sink_id = sink_id;
  AddIssue(issue_info);
}

void MediaRouterUI::SendIssueForScreenPermission(const MediaSink::Id& sink_id) {
#if BUILDFLAG(IS_MAC)
  std::string issue_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_MAC_SCREEN_CAPTURE_PERMISSION_ERROR);
  IssueInfo issue_info(issue_title, IssueInfo::Action::DISMISS,
                       IssueInfo::Severity::WARNING);
  issue_info.sink_id = sink_id;
  AddIssue(issue_info);
#else
  NOTREACHED() << "Only valid for MAC OS!";
#endif
}

void MediaRouterUI::SendIssueForUnableToCast(MediaCastMode cast_mode,
                                             const MediaSink::Id& sink_id) {
  // For a generic error, claim a tab error unless it was specifically desktop
  // mirroring.
  std::string issue_title =
      (cast_mode == MediaCastMode::DESKTOP_MIRROR)
          ? l10n_util::GetStringUTF8(
                IDS_MEDIA_ROUTER_ISSUE_UNABLE_TO_CAST_DESKTOP)
          : l10n_util::GetStringUTF8(
                IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_FOR_TAB);
  IssueInfo issue_info(issue_title, IssueInfo::Action::DISMISS,
                       IssueInfo::Severity::WARNING);
  issue_info.sink_id = sink_id;
  AddIssue(issue_info);
}

void MediaRouterUI::SendIssueForTabAudioNotSupported(
    const MediaSink::Id& sink_id) {
  IssueInfo issue_info(
      l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_ISSUE_TAB_AUDIO_NOT_SUPPORTED),
      IssueInfo::Action::DISMISS, IssueInfo::Severity::NOTIFICATION);
  issue_info.sink_id = sink_id;
  AddIssue(issue_info);
}

IssueManager* MediaRouterUI::GetIssueManager() {
  return GetMediaRouter()->GetIssueManager();
}

void MediaRouterUI::StartObservingIssues() {
  issues_observer_ =
      std::make_unique<UiIssuesObserver>(GetIssueManager(), this);
  issues_observer_->Init();
}

void MediaRouterUI::OnIssue(const Issue& issue) {
  issue_ = issue;
  UpdateSinks();
}

void MediaRouterUI::OnIssueCleared() {
  issue_ = absl::nullopt;
  UpdateSinks();
}

void MediaRouterUI::OnRoutesUpdated(const std::vector<MediaRoute>& routes) {
  routes_.clear();

  for (const MediaRoute& route : routes) {
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

  if (terminating_route_id_ &&
      std::find_if(
          routes.begin(), routes.end(), [this](const MediaRoute& route) {
            return route.media_route_id() == terminating_route_id_.value();
          }) == routes.end()) {
    terminating_route_id_.reset();
  }
  UpdateSinks();
}

void MediaRouterUI::OnSinksUpdated(
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

void MediaRouterUI::OnRouteResponseReceived(
    int route_request_id,
    const MediaSink::Id& sink_id,
    MediaCastMode cast_mode,
    const std::u16string& presentation_request_source_name,
    const RouteRequestResult& result) {
  // If we receive a new route that we aren't expecting, do nothing.
  if (!current_route_request_ || route_request_id != current_route_request_->id)
    return;

  const MediaRoute* route = result.route();
  if (!route) {
    // The provider will handle sending an issue for a failed route request.
    logger_->LogError(mojom::LogCategory::kUi, kLoggerComponent,
                      "MediaRouteResponse returned error: " + result.error(),
                      sink_id, "", "");
  }

  current_route_request_.reset();
  if (result.result_code() == RouteRequestResult::OK &&
      cast_mode == TAB_MIRROR && !base::TimeTicks::IsHighResolution()) {
    // When tab mirroring on a device without a high resolution clock, the audio
    // is not mirrored.
    SendIssueForTabAudioNotSupported(sink_id);
  } else if (result.result_code() == RouteRequestResult::TIMED_OUT) {
    SendIssueForRouteTimeout(cast_mode, sink_id,
                             presentation_request_source_name);
  }
}

void MediaRouterUI::UpdateModelHeader() {
  const std::u16string source_name = GetPresentationRequestSourceName();
  const std::u16string header_text =
      source_name.empty()
          ? l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TAB_MIRROR_CAST_MODE)
          : l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_PRESENTATION_CAST_MODE,
                                       source_name);
  model_.set_dialog_header(header_text);
  for (CastDialogController::Observer& observer : observers_)
    observer.OnModelUpdated(model_);
}

UIMediaSink MediaRouterUI::ConvertToUISink(const MediaSinkWithCastModes& sink,
                                           const MediaRoute* route,
                                           const absl::optional<Issue>& issue) {
  UIMediaSink ui_sink{sink.sink.provider_id()};
  ui_sink.id = sink.sink.id();
  ui_sink.friendly_name = GetSinkFriendlyName(sink.sink);
  ui_sink.icon_type = sink.sink.icon_type();
  ui_sink.cast_modes = sink.cast_modes;

  if (route) {
    ui_sink.status_text = base::UTF8ToUTF16(route->description());
    ui_sink.route = *route;
    if (terminating_route_id_ &&
        route->media_route_id() == terminating_route_id_.value()) {
      ui_sink.state = UIMediaSinkState::DISCONNECTING;
    } else if (route->is_connecting()) {
      ui_sink.state = UIMediaSinkState::CONNECTING;
    } else {
      ui_sink.state = UIMediaSinkState::CONNECTED;
    }
  } else {
    ui_sink.state = current_route_request() &&
                            sink.sink.id() == current_route_request()->sink_id
                        ? UIMediaSinkState::CONNECTING
                        : UIMediaSinkState::AVAILABLE;
  }
  if (issue && IssueMatches(*issue, ui_sink))
    ui_sink.issue = issue;
  return ui_sink;
}

MediaRouter* MediaRouterUI::GetMediaRouter() const {
  return MediaRouterFactory::GetApiForBrowserContext(
      initiator_->GetBrowserContext());
}

Browser* MediaRouterUI::GetBrowser() {
  return chrome::FindBrowserWithWebContents(initiator_);
}

void MediaRouterUI::SimulateDocumentAvailableForTest() {
  DCHECK(web_contents_observer_for_test_);
  web_contents_observer_for_test_->DidFinishNavigation(nullptr);
}

}  // namespace media_router
