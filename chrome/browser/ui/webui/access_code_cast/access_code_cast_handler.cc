// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <numeric>

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_runner_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"
#include "components/media_router/common/mojom/media_router_mojom_traits.h"
#include "components/sessions/content/session_tab_helper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

using media_router::mojom::RouteRequestResultCode;
// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

namespace {

const char* AddSinkResultCodeToStringHelper(AddSinkResultCode value) {
  switch (value) {
    case AddSinkResultCode::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    case AddSinkResultCode::OK:
      return "OK";
    case AddSinkResultCode::AUTH_ERROR:
      return "AUTH_ERROR";
    case AddSinkResultCode::HTTP_RESPONSE_CODE_ERROR:
      return "HTTP_RESPONSE_CODE_ERROR";
    case AddSinkResultCode::RESPONSE_MALFORMED:
      return "RESPONSE_MALFORMED";
    case AddSinkResultCode::EMPTY_RESPONSE:
      return "EMPTY_RESPONSE";
    case AddSinkResultCode::INVALID_ACCESS_CODE:
      return "INVALID_ACCESS_CODE";
    case AddSinkResultCode::ACCESS_CODE_NOT_FOUND:
      return "ACCESS_CODE_NOT_FOUND";
    case AddSinkResultCode::TOO_MANY_REQUESTS:
      return "TOO_MANY_REQUESTS";
    case AddSinkResultCode::SERVICE_NOT_PRESENT:
      return "SERVICE_NOT_PRESENT";
    case AddSinkResultCode::SERVER_ERROR:
      return "SERVER_ERROR";
    case AddSinkResultCode::SINK_CREATION_ERROR:
      return "SINK_CREATION_ERROR";
    case AddSinkResultCode::CHANNEL_OPEN_ERROR:
      return "CHANNEL_OPEN_ERROR";
    case AddSinkResultCode::PROFILE_SYNC_ERROR:
      return "PROFILE_SYNC_ERROR";
    default:
      return nullptr;
  }
}

std::string AddSinkResultCodeToString(AddSinkResultCode value) {
  const char* str = AddSinkResultCodeToStringHelper(value);
  if (!str) {
    return base::StringPrintf("Unknown AddSinkResultCode value: %i",
                              static_cast<int32_t>(value));
  }
  return str;
}

constexpr char kLoggerComponent[] = "AccessCodeCastHandler";

std::string CastModeSetToString(
    const media_router::CastModeSet& cast_mode_set) {
  return "{" +
         std::accumulate(cast_mode_set.begin(), cast_mode_set.end(),
                         std::string{},
                         [](const std::string& a, MediaCastMode b) {
                           return a.empty() ? base::NumberToString(b)
                                            : a + ',' + base::NumberToString(b);
                         }) +
         "}";
}

}  // namespace

// TODO(b/201430609) - refactor this class to split out AddSink and StartCasting
// functionality.
AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    Profile* profile,
    MediaRouter* media_router,
    const media_router::CastModeSet& cast_mode_set,
    content::WebContents* web_contents,
    std::unique_ptr<StartPresentationContext> start_presentation_context)
    : AccessCodeCastHandler(
          std::move(page_handler),
          std::move(page),
          profile,
          media_router,
          cast_mode_set,
          web_contents,
          std::move(start_presentation_context),
          AccessCodeCastSinkServiceFactory::GetForProfile(profile)) {}

AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    Profile* profile,
    MediaRouter* media_router,
    const media_router::CastModeSet& cast_mode_set,
    content::WebContents* web_contents,
    std::unique_ptr<StartPresentationContext> start_presentation_context,
    AccessCodeCastSinkService* access_code_sink_service)
    : page_(std::move(page)),
      receiver_(this, std::move(page_handler)),
      profile_(profile),
      media_router_(media_router),
      cast_mode_set_(cast_mode_set),
      web_contents_(web_contents),
      start_presentation_context_(std::move(start_presentation_context)),
      presentation_manager_(
          web_contents ? WebContentsPresentationManager::Get(web_contents)
                       : nullptr),
      access_code_sink_service_(access_code_sink_service) {
  DCHECK(profile_) << "Must have profile!";
  DCHECK(access_code_sink_service_)
      << "AccessCodeSinkService was not properly created!";

  if (media_router_) {
    media_router_->OnUserGesture();
    InitMediaSources();
  }

  if (presentation_manager_) {
    presentation_manager_->AddObserver(this);
  }
}

AccessCodeCastHandler::~AccessCodeCastHandler() {
  if (query_result_manager_.get()) {
    query_result_manager_->RemoveObserver(this);
  }
  if (presentation_manager_) {
    presentation_manager_->RemoveObserver(this);
  }

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

void AccessCodeCastHandler::AddSink(
    const std::string& access_code,
    access_code_cast::mojom::CastDiscoveryMethod discovery_method,
    AddSinkCallback callback) {
  add_sink_callback_ = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), AddSinkResultCode::UNKNOWN_ERROR);
  DCHECK(media_router_) << "Must have media router!";

  access_code_sink_service_->DiscoverSink(
      access_code, base::BindOnce(&AccessCodeCastHandler::OnSinkAddedResult,
                                  weak_ptr_factory_.GetWeakPtr()));
}

bool AccessCodeCastHandler::IsCastModeAvailable(MediaCastMode mode) const {
  return base::Contains(cast_mode_set_, mode);
}

void AccessCodeCastHandler::InitMediaSources() {
  DCHECK(!query_result_manager_)
      << "Should only init query results manager once!";
  DCHECK(media_router_) << "Must have media router!";

  query_result_manager_ = std::make_unique<QueryResultManager>(media_router_);
  query_result_manager_->AddObserver(this);

  InitPresentationSources();
  InitMirroringSources();
}

void AccessCodeCastHandler::InitPresentationSources() {
  if (!IsCastModeAvailable(MediaCastMode::PRESENTATION)) {
    // No need to bother if presentation isn't an option.
    return;
  }
  if (start_presentation_context_) {
    OnDefaultPresentationChanged(
        &start_presentation_context_->presentation_request());
  } else if (presentation_manager_ &&
             presentation_manager_->HasDefaultPresentationRequest()) {
    OnDefaultPresentationChanged(
        &presentation_manager_->GetDefaultPresentationRequest());
  }
}

void AccessCodeCastHandler::InitMirroringSources() {
  // Use a placeholder URL as origin for mirroring.
  url::Origin origin = url::Origin::Create(GURL());

  if (IsCastModeAvailable(MediaCastMode::DESKTOP_MIRROR)) {
    query_result_manager_->SetSourcesForCastMode(
        MediaCastMode::DESKTOP_MIRROR, {MediaSource::ForUnchosenDesktop()},
        origin);
  }

  if (IsCastModeAvailable(MediaCastMode::TAB_MIRROR)) {
    SessionID::id_type tab_id =
        sessions::SessionTabHelper::IdForTab(web_contents_).id();
    if (tab_id != -1) {
      MediaSource mirroring_source(MediaSource::ForTab(tab_id));
      query_result_manager_->SetSourcesForCastMode(MediaCastMode::TAB_MIRROR,
                                                   {mirroring_source}, origin);
    }
  }
}

// Discovery is not complete until the sink is in QRM. This is because any
// attempt to create route parameters before the sink is in QRM will fail.
void AccessCodeCastHandler::CheckForDiscoveryCompletion() {
  // Dialog  has already notified (with most likely an error).
  if (!add_sink_callback_)
    return;
  DCHECK(sink_id_) << "Must have a sink id to complete!";

  // Verify that the sink is in QRM.
  if (std::find_if(cast_mode_set_.begin(), cast_mode_set_.end(),
                   [this](MediaCastMode cast_mode) {
                     return query_result_manager_->GetSourceForCastModeAndSink(
                         cast_mode, *sink_id_);
                   }) == cast_mode_set_.end()) {
    // sink hasn't been added to QRM yet.
    return;
  }

  // Sink has been completely added so caller can be alerted.
  if (base::FeatureList::IsEnabled(features::kAccessCodeCastRememberDevices)) {
    access_code_sink_service_->StoreSinkInPrefsById(sink_id_.value());
  }
  std::move(add_sink_callback_).Run(AddSinkResultCode::OK);
}

void AccessCodeCastHandler::OnSinkAddedResult(
    access_code_cast::mojom::AddSinkResultCode add_sink_result,
    absl::optional<MediaSink::Id> sink_id) {
  DCHECK(sink_id || add_sink_result != AddSinkResultCode::OK);
  // Wait for OnResultsUpdated before triggering the |add_sink_callback_| since
  // we are not entirely sure the sink is ready to be casted to yet.
  if (add_sink_result != AddSinkResultCode::OK && add_sink_callback_) {
    auto error_message =
        std::string("The device could not be added because of enum error : ") +
        AddSinkResultCodeToString(add_sink_result);
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kUi, kLoggerComponent, error_message, "", "", "");
    std::move(add_sink_callback_).Run(add_sink_result);
  }
  if (sink_id) {
    sink_id_ = sink_id;
  }
  CheckForDiscoveryCompletion();
}

void AccessCodeCastHandler::SetSinkCallbackForTesting(
    AddSinkCallback callback) {
  add_sink_callback_ = std::move(callback);
}

// QueryManager observer that alerts the handler about the availability of
// newly discovered sinks, as well as what types of casting those sinks support.
void AccessCodeCastHandler::OnSinksUpdated(
    const std::vector<MediaSinkWithCastModes>& sinks) {
  if (add_sink_callback_ && sink_id_) {
    media_router_->GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The QueryManager observer has been alerted about the availability of "
        "newly discovered sinks.",
        sink_id_.value(), "", "");
    CheckForDiscoveryCompletion();
  }
}

void AccessCodeCastHandler::CastToSink(CastToSinkCallback callback) {
  DCHECK(media_router_) << "Must have media router!";
  DCHECK(sink_id_) << "Cast called without a valid sink id!";

  media_router_->GetLogger()->LogInfo(
      mojom::LogCategory::kUi, kLoggerComponent,
      "CreateRoute requested by AccessCodeCastHandler.", sink_id_.value(), "",
      "");

  absl::optional<RouteParameters> params;
  MediaCastMode cast_mode;

  // Walk through the available cast modes, and look for one for that matches
  // the available sources. First match wins. params & cast_mode should be set
  // to valid values if this loop completes successfully.
  for (auto it = cast_mode_set_.begin(); it != cast_mode_set_.end() && !params;
       ++it) {
    cast_mode = *it;
    params = GetRouteParameters(cast_mode);
  }

  if (!params) {
    media_router_->GetLogger()->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "No corresponding MediaSource for cast modes " +
            CastModeSetToString(cast_mode_set_),
        sink_id_.value(), "", "");
    std::move(callback).Run(RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  if (RequiresScreenCapturePermission(cast_mode)) {
    const bool screen_capture_allowed = GetScreenCapturePermission();
    if (!screen_capture_allowed) {
      media_router_->GetLogger()->LogError(
          mojom::LogCategory::kUi, kLoggerComponent,
          "Screen capture is not allowed. The Route "
          "Request has been cancelled",
          sink_id_.value(), "", "");
      std::move(callback).Run(RouteRequestResultCode::CANCELLED);
      return;
    }
  }

  media_router_->CreateRoute(
      params->source_id, sink_id_.value(), params->origin, web_contents_,
      base::BindOnce(&AccessCodeCastHandler::OnRouteResponse,
                     weak_ptr_factory_.GetWeakPtr(), cast_mode,
                     current_route_request_->id, sink_id_.value(),
                     std::move(params->presentation_callback),
                     std::move(callback)),
      params->timeout, params->off_the_record);
}

absl::optional<RouteParameters> AccessCodeCastHandler::GetRouteParameters(
    MediaCastMode cast_mode) {
  DCHECK(query_result_manager_) << "Initialize Query Result Manager first!";
  DCHECK(sink_id_) << "Must have a sink id!";
  RouteParameters params;

  std::unique_ptr<MediaSource> source =
      query_result_manager_->GetSourceForCastModeAndSink(cast_mode,
                                                         sink_id_.value());
  if (!source) {
    return absl::nullopt;
  }
  params.source_id = source->id();

  bool for_presentation_source = cast_mode == MediaCastMode::PRESENTATION;
  if (for_presentation_source && !presentation_request_) {
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kUi, kLoggerComponent,
        "Requested to create a route for presentation, but "
        "presentation request is missing.",
        sink_id_.value(), source->id(), "");
    return absl::nullopt;
  }

  current_route_request_ =
      absl::make_optional<MediaRouterUI::RouteRequest>(sink_id_.value());
  params.origin = for_presentation_source ? presentation_request_->frame_origin
                                          : url::Origin::Create(GURL());

  if (for_presentation_source) {
    if (start_presentation_context_) {
      params.presentation_callback =
          base::BindOnce(&StartPresentationContext::HandleRouteResponse,
                         std::move(start_presentation_context_));
    } else if (presentation_manager_) {
      params.presentation_callback = base::BindOnce(
          &WebContentsPresentationManager::OnPresentationResponse,
          presentation_manager_, *presentation_request_);
    } else {
      NOTREACHED();
    }
  }

  params.timeout = GetRouteRequestTimeout(cast_mode);
  params.off_the_record =
      web_contents_ && web_contents_->GetBrowserContext()->IsOffTheRecord();

  return absl::make_optional(std::move(params));
}

// MediaRouter::CreateRoute callback handler - log the success / failure of the
// CreateRoute operation and return the result code to the dialog.
// If there is a presentation request, call the appropriate presentation
// callbacks with the presentation connection.
void AccessCodeCastHandler::OnRouteResponse(
    MediaCastMode cast_mode,
    int route_request_id,
    const MediaSink::Id& sink_id,
    MediaRouteResponseCallback presentation_callback,
    CastToSinkCallback dialog_callback,
    mojom::RoutePresentationConnectionPtr connection,
    const RouteRequestResult& result) {
  // Only respond to expected responses.
  if (!current_route_request_ ||
      route_request_id != current_route_request_->id) {
    return;
  }
  current_route_request_.reset();

  if (presentation_callback) {
    std::move(presentation_callback).Run(std::move(connection), result);
  }

  const MediaRoute* route = result.route();
  if (!route) {
    DCHECK(result.result_code() != RouteRequestResult::OK)
        << "No route but OK response";
    // The provider will handle sending an issue for a failed route request.
    media_router_->GetLogger()->LogError(
        mojom::LogCategory::kUi, kLoggerComponent,
        "MediaRouteResponse returned error: " + result.error(), sink_id, "",
        "");
    std::move(dialog_callback)
        .Run(mojo::EnumTraits<
             RouteRequestResultCode,
             RouteRequestResult::ResultCode>::ToMojom(result.result_code()));
    return;
  }

  base::UmaHistogramSparse("MediaRouter.Source.CastingSource", cast_mode);
  std::move(dialog_callback).Run(RouteRequestResultCode::OK);
}

// When a new presentation source is obtained, pass that on to the
// QueryResultManager.
void AccessCodeCastHandler::OnDefaultPresentationChanged(
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
}

// If no presentation source remains, remove presentation mode from what
// QueryResultManager will offer.
void AccessCodeCastHandler::OnDefaultPresentationRemoved() {
  presentation_request_.reset();
  query_result_manager_->RemoveSourcesForCastMode(MediaCastMode::PRESENTATION);
}

}  // namespace media_router
