// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui.h"

#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/webui/media_router/web_contents_display_observer.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/issue_manager.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/browser/log_util.h"
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
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"

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

void MaybeReportCastingSource(MediaCastMode cast_mode,
                              const RouteRequestResult& result) {
  if (result.result_code() == mojom::RouteRequestResultCode::OK)
    base::UmaHistogramSparse("MediaRouter.Source.CastingSource", cast_mode);
}

const CastModeSet CreateMediaCastModeSet(const MediaCastMode& cast_mode) {
  if (base::FeatureList::IsEnabled(
          media_router::kFallbackToAudioTabMirroring)) {
    // On sinks that do not support remote playback or presentation, we allow
    // falling back to tab mirroring.
    return {cast_mode, MediaCastMode::TAB_MIRROR};
  }
  return {cast_mode};
}

}  // namespace

MediaRouterUI::MediaRouterUI(
    std::unique_ptr<MediaRouteStarter> media_route_starter)
    : media_route_starter_(std::move(media_route_starter)),
      router_(media_route_starter_->GetMediaRouter()),
      logger_(router_->GetLogger()) {
  DCHECK(media_route_starter_) << "Must have a media_route_starter!";
  media_route_starter_->SetLoggerComponent(kLoggerComponent);
  Init();
}

MediaRouterUI::~MediaRouterUI() {
  StopObservingMirroringMediaControllerHosts();
  if (media_route_starter_)
    DetachFromMediaRouteStarter();
  for (CastDialogController::Observer& observer : observers_) {
    observer.OnControllerDestroying();
  }
}

// static
std::unique_ptr<MediaRouterUI> MediaRouterUI::CreateMediaRouterUI(
    MediaRouterUIParameters params) {
  DCHECK(params.initiator) << "Must have an initiator!";
  return std::make_unique<MediaRouterUI>(
      std::make_unique<MediaRouteStarter>(std::move(params)));
}

std::unique_ptr<MediaRouterUI> MediaRouterUI::CreateWithDefaultMediaSource(
    content::WebContents* initiator) {
  return CreateMediaRouterUI(MediaRouterUIParameters(
      CreateMediaCastModeSet(MediaCastMode::PRESENTATION), initiator));
}

// static
std::unique_ptr<MediaRouterUI>
MediaRouterUI::CreateWithDefaultMediaSourceAndMirroring(
    content::WebContents* initiator) {
  return CreateMediaRouterUI(MediaRouterUIParameters(
      {MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR,
       MediaCastMode::DESKTOP_MIRROR},
      initiator));
}

// static
std::unique_ptr<MediaRouterUI>
MediaRouterUI::CreateWithStartPresentationContext(
    content::WebContents* initiator,
    std::unique_ptr<StartPresentationContext> context) {
  DCHECK(context) << "context must not be null!";
  return CreateMediaRouterUI(MediaRouterUIParameters(
      CreateMediaCastModeSet(MediaCastMode::PRESENTATION), initiator,
      std::move(context)));
}

// static
std::unique_ptr<MediaRouterUI>
MediaRouterUI::CreateWithStartPresentationContextAndMirroring(
    content::WebContents* initiator,
    std::unique_ptr<StartPresentationContext> context) {
  DCHECK(context) << "context must not be null!";
  return CreateMediaRouterUI(MediaRouterUIParameters(
      {MediaCastMode::PRESENTATION, MediaCastMode::TAB_MIRROR,
       MediaCastMode::DESKTOP_MIRROR},
      initiator, std::move(context)));
}

// static
std::unique_ptr<MediaRouterUI>
MediaRouterUI::CreateWithMediaSessionRemotePlayback(
    content::WebContents* initiator,
    media::VideoCodec video_codec,
    media::AudioCodec audio_codec) {
  DCHECK(video_codec != media::VideoCodec::kUnknown) << "Unknown video codec.";
  DCHECK(audio_codec != media::AudioCodec::kUnknown) << "Unknown audio codec.";
  return CreateMediaRouterUI(MediaRouterUIParameters(
      CreateMediaCastModeSet(MediaCastMode::REMOTE_PLAYBACK), initiator,
      nullptr, video_codec, audio_codec));
}

void MediaRouterUI::DetachFromMediaRouteStarter() {
  media_route_starter()->RemovePresentationRequestSourceObserver(this);
  media_route_starter()->RemoveMediaSinkWithCastModesObserver(this);
}

void MediaRouterUI::AddObserver(CastDialogController::Observer* observer) {
  observers_.AddObserver(observer);
  // TODO(takumif): Update the header when this object is initialized instead.
  UpdateModelHeader(media_route_starter()->GetPresentationRequestSourceName());
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

void MediaRouterUI::FreezeRoute(const std::string& route_id) {
  MirroringMediaControllerHost* freeze_host =
      GetMediaRouter()->GetMirroringMediaControllerHost(route_id);
  if (!freeze_host) {
    return;
  }

  freeze_host->Freeze();
}

void MediaRouterUI::UnfreezeRoute(const std::string& route_id) {
  MirroringMediaControllerHost* freeze_host =
      GetMediaRouter()->GetMirroringMediaControllerHost(route_id);
  if (!freeze_host) {
    return;
  }

  freeze_host->Unfreeze();
}

std::unique_ptr<MediaRouteStarter> MediaRouterUI::TakeMediaRouteStarter() {
  DCHECK(media_route_starter_) << "MediaRouteStarter already taken!";
  DetachFromMediaRouteStarter();
  auto starter = std::move(media_route_starter_);
  if (destructor_) {
    std::move(destructor_).Run();  // May destroy `this`.
  }
  return starter;
}

void MediaRouterUI::RegisterDestructor(base::OnceClosure destructor) {
  DCHECK(!destructor_);
  destructor_ = std::move(destructor);
}

bool MediaRouterUI::CreateRoute(const MediaSink::Id& sink_id,
                                MediaCastMode cast_mode) {
  logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                   "CreateRoute requested by MediaRouterViewsUI.", sink_id, "",
                   "");

  auto params =
      media_route_starter()->CreateRouteParameters(sink_id, cast_mode);
  if (!params) {
    SendIssueForUnableToCast(cast_mode, sink_id);
    return false;
  }

  if (!MediaRouteStarter::GetScreenCapturePermission(cast_mode)) {
    SendIssueForScreenPermission(sink_id);
    return false;
  }

  GetIssueManager()->ClearAllIssues();

  current_route_request_ = std::make_optional(*params->request);

  // Note that `route_result_callbacks` don't get called when MediaRoterUI is
  // destroyed before the route is created, e.g. when the Cast dialog is closed
  // when the desktop picker is shown.
  params->route_result_callbacks.push_back(
      base::BindOnce(&MaybeReportCastingSource, cast_mode));

  params->route_result_callbacks.push_back(base::BindOnce(
      &MediaRouterUI::OnRouteResponseReceived, weak_factory_.GetWeakPtr(),
      current_route_request_->id, sink_id, cast_mode,
      media_route_starter()->GetPresentationRequestSourceName()));

  media_route_starter()->StartRoute(std::move(params));

  // TODO(crbug.com/40103608): This call to UpdateSinks() was originally in
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
  std::erase_if(enabled_sinks,
                [&display_sink_id](const MediaSinkWithCastModes& sink) {
                  return sink.sink.id() == display_sink_id;
                });

  return enabled_sinks;
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
    sink_ids.push_back(std::string(log_util::TruncateId(sink.sink.id())));
  }

  logger_->LogInfo(
      mojom::LogCategory::kUi, kLoggerComponent,
      base::StrCat(
          {base::StringPrintf("%zu sinks shown on CastDialogView closed: ",
                              sink_ids.size()),
           base::JoinString(sink_ids, ",")}),
      "", "", "");
}

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

void MediaRouterUI::Init() {
  DCHECK(!collator_) << "Init should only be called once!";

  media_route_starter()->AddMediaSinkWithCastModesObserver(this);
  media_route_starter()->AddPresentationRequestSourceObserver(this);

  GetMediaRouter()->OnUserGesture();

  UErrorCode error = U_ZERO_ERROR;
  const std::string& locale = g_browser_process->GetApplicationLocale();
  collator_.reset(
      icu::Collator::createInstance(icu::Locale(locale.c_str()), error));
  if (U_FAILURE(error)) {
    DLOG(ERROR) << "Failed to create collator for locale " << locale;
    collator_.reset();
  }

  // Get the current list of media routes, so that the WebUI will have routes
  // information at initialization.
  OnRoutesUpdated(GetMediaRouter()->GetCurrentRoutes());
  display_observer_ = WebContentsDisplayObserver::Create(
      initiator(),
      base::BindRepeating(&MediaRouterUI::UpdateSinks, base::Unretained(this)));

  routes_observer_ = std::make_unique<UIMediaRoutesObserver>(
      GetMediaRouter(), base::BindRepeating(&MediaRouterUI::OnRoutesUpdated,
                                            base::Unretained(this)));

  StartObservingIssues();
}

void MediaRouterUI::OnSourceUpdated(std::u16string& source_name) {
  UpdateModelHeader(source_name);
}

void MediaRouterUI::OnFreezeInfoChanged() {
  // UpdateSinks regenerates the list of UIMediaSinks, and for each it queries
  // the current freeze info.
  UpdateSinks();
}

void MediaRouterUI::UpdateSinks() {
  if (base::FeatureList::IsEnabled(kShowCastPermissionRejectedError) &&
      issue_.has_value() && issue_->is_permission_rejected_issue()) {
    // Clean up the discovered sinks if the permission is rejected.
    model_.set_media_sinks({});
    model_.set_is_permission_rejected(true);
  } else {
    std::vector<UIMediaSink> media_sinks;
    for (const MediaSinkWithCastModes& sink : GetEnabledSinks()) {
      auto route_it = base::ranges::find(routes(), sink.sink.id(),
                                         &MediaRoute::media_sink_id);
      const MediaRoute* route =
          route_it == routes().end() ? nullptr : &*route_it;
      media_sinks.push_back(ConvertToUISink(sink, route, issue_));
    }
    model_.set_media_sinks(std::move(media_sinks));
  }

  for (CastDialogController::Observer& observer : observers_)
    observer.OnModelUpdated(model_);
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
      issue_title = l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT_WITH_HOSTNAME,
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
    case REMOTE_PLAYBACK:
      issue_title =
          l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_TIMEOUT);
      break;
  }

  IssueInfo issue_info(issue_title, IssueInfo::Severity::NOTIFICATION, sink_id);
  AddIssue(issue_info);
}

std::u16string MediaRouterUI::GetSinkFriendlyNameFromId(
    const MediaSink::Id& sink_id) {
  for (const MediaSinkWithCastModes& sink : GetEnabledSinks()) {
    if (sink.sink.id() == sink_id) {
      return base::UTF8ToUTF16(sink.sink.name());
    }
  }
  return std::u16string(u"Device");
}

void MediaRouterUI::SendIssueForUserNotAllowed(const MediaSink::Id& sink_id) {
  std::string issue_title = l10n_util::GetStringFUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_USER_NOT_ALLOWED,
      GetSinkFriendlyNameFromId(sink_id));
  IssueInfo issue_info(issue_title, IssueInfo::Severity::WARNING, sink_id);
  AddIssue(issue_info);
}

void MediaRouterUI::SendIssueForNotificationDisabled(
    const MediaSink::Id& sink_id) {
  std::string issue_title = l10n_util::GetStringFUTF8(
      IDS_MEDIA_ROUTER_ISSUE_CREATE_ROUTE_NOTIFICATION_DISABLED,
      GetSinkFriendlyNameFromId(sink_id));
  IssueInfo issue_info(issue_title, IssueInfo::Severity::WARNING, sink_id);
  AddIssue(issue_info);
}

void MediaRouterUI::SendIssueForScreenPermission(const MediaSink::Id& sink_id) {
#if BUILDFLAG(IS_MAC)
  std::string issue_title = l10n_util::GetStringUTF8(
      IDS_MEDIA_ROUTER_ISSUE_MAC_SCREEN_CAPTURE_PERMISSION_ERROR);
  IssueInfo issue_info(issue_title, IssueInfo::Severity::WARNING, sink_id);
  AddIssue(issue_info);
#else
  NOTREACHED_IN_MIGRATION() << "Only valid for MAC OS!";
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
  IssueInfo issue_info(issue_title, IssueInfo::Severity::WARNING, sink_id);
  AddIssue(issue_info);
}

void MediaRouterUI::SendIssueForTabAudioNotSupported(
    const MediaSink::Id& sink_id) {
  IssueInfo issue_info(
      l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_ISSUE_TAB_AUDIO_NOT_SUPPORTED),
      IssueInfo::Severity::NOTIFICATION, sink_id);
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
  issue_ = std::nullopt;
  UpdateSinks();
}

void MediaRouterUI::OnRoutesUpdated(const std::vector<MediaRoute>& routes) {
  StopObservingMirroringMediaControllerHosts();
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
    MirroringMediaControllerHost* mirroring_controller_host =
        GetMediaRouter()->GetMirroringMediaControllerHost(
            route.media_route_id());
    if (mirroring_controller_host) {
      mirroring_controller_host->AddObserver(this);
    }
  }

  if (terminating_route_id_ &&
      !base::Contains(routes, terminating_route_id_.value(),
                      &MediaRoute::media_route_id)) {
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
  if (result.result_code() == mojom::RouteRequestResultCode::OK) {
    for (CastDialogController::Observer& observer : observers_) {
      observer.OnCastingStarted();
    }
  }

  if (result.result_code() == mojom::RouteRequestResultCode::OK &&
      cast_mode == TAB_MIRROR && !base::TimeTicks::IsHighResolution()) {
    // When tab mirroring on a device without a high resolution clock, the audio
    // is not mirrored.
    SendIssueForTabAudioNotSupported(sink_id);
  } else if (result.result_code() == mojom::RouteRequestResultCode::TIMED_OUT) {
    SendIssueForRouteTimeout(cast_mode, sink_id,
                             presentation_request_source_name);
  } else if (result.result_code() ==
             mojom::RouteRequestResultCode::USER_NOT_ALLOWED) {
    SendIssueForUserNotAllowed(sink_id);
  } else if (result.result_code() ==
             mojom::RouteRequestResultCode::NOTIFICATION_DISABLED) {
    SendIssueForNotificationDisabled(sink_id);
  }
}

void MediaRouterUI::UpdateModelHeader(const std::u16string& source_name) {
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
                                           const std::optional<Issue>& issue) {
  UIMediaSink ui_sink{sink.sink.provider_id()};
  ui_sink.id = sink.sink.id();
  ui_sink.friendly_name = base::UTF8ToUTF16(sink.sink.name());
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

      MirroringMediaControllerHost* mirroring_controller_host =
          GetMediaRouter()->GetMirroringMediaControllerHost(
              route->media_route_id());
      if (mirroring_controller_host) {
        ui_sink.freeze_info.can_freeze = mirroring_controller_host->CanFreeze();
        ui_sink.freeze_info.is_frozen = mirroring_controller_host->IsFrozen();
      }
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

void MediaRouterUI::StopObservingMirroringMediaControllerHosts() {
  for (const MediaRoute& route : routes_) {
    MirroringMediaControllerHost* mirroring_controller_host =
        GetMediaRouter()->GetMirroringMediaControllerHost(
            route.media_route_id());
    if (mirroring_controller_host) {
      // It is safe to call RemoveObserver even if we are not observing a
      // particular host.
      mirroring_controller_host->RemoveObserver(this);
    }
  }
}

MediaRouter* MediaRouterUI::GetMediaRouter() const {
  return router_;
}

}  // namespace media_router
