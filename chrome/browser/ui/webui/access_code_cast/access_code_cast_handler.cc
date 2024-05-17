// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <numeric>

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
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
    case AddSinkResultCode::INTERNAL_MEDIA_ROUTER_ERROR:
      return "INTERNAL_MEDIA_ROUTER_ERROR";
    default:
      return nullptr;
  }
}

AccessCodeCastCastMode CastModeMetricsHelper(MediaCastMode mode) {
  switch (mode) {
    case MediaCastMode::PRESENTATION:
      return AccessCodeCastCastMode::kPresentation;
    case MediaCastMode::TAB_MIRROR:
      return AccessCodeCastCastMode::kTabMirror;
    case MediaCastMode::DESKTOP_MIRROR:
      return AccessCodeCastCastMode::kDesktopMirror;
    case MediaCastMode::REMOTE_PLAYBACK:
      return AccessCodeCastCastMode::kRemotePlayback;
    default:
      NOTREACHED_IN_MIGRATION();
      return AccessCodeCastCastMode::kPresentation;
  }
}

AddSinkResultCode AddSinkMetricsCallback(AddSinkResultCode result) {
  AccessCodeCastMetrics::RecordAddSinkResult(
      false, AddSinkResultMetricsHelper(result));
  return result;
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

AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    const media_router::CastModeSet& cast_mode_set,
    std::unique_ptr<MediaRouteStarter> media_route_starter)
    : page_(std::move(page)),
      receiver_(this, std::move(page_handler)),
      cast_mode_set_(cast_mode_set),
      media_route_starter_(std::move(media_route_starter)) {
  if (media_route_starter_) {
    DCHECK(media_route_starter_->GetProfile())
        << "The MediaRouteStarter does not have a valid profile!";

    // Ensure we don't use an off-the-record profile.
    access_code_sink_service_ = AccessCodeCastSinkServiceFactory::GetForProfile(
        media_route_starter_->GetProfile()->GetOriginalProfile());
    DCHECK(access_code_sink_service_)
        << "AccessCodeSinkService was not properly created!";

    identity_manager_ = IdentityManagerFactory::GetForProfile(
        media_route_starter_->GetProfile()->GetOriginalProfile());

    sync_service_ = SyncServiceFactory::GetForProfile(
        media_route_starter_->GetProfile()->GetOriginalProfile());
    Init();
  }
}

AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    const media_router::CastModeSet& cast_mode_set,
    std::unique_ptr<MediaRouteStarter> media_route_starter,
    AccessCodeCastSinkService* access_code_sink_service)
    : page_(std::move(page)),
      receiver_(this, std::move(page_handler)),
      cast_mode_set_(cast_mode_set),
      media_route_starter_(std::move(media_route_starter)),
      access_code_sink_service_(access_code_sink_service) {
  Init();
}

AccessCodeCastHandler::~AccessCodeCastHandler() {
  AccessCodeCastMetrics::RecordAccessCodeNotFoundCount(
      access_code_not_found_count_);
  if (media_route_starter_)
    media_route_starter_->RemoveMediaSinkWithCastModesObserver(this);
}

void AccessCodeCastHandler::Init() {
  DCHECK(media_route_starter_) << "Must have MediaRouterService!";
  media_route_starter_->SetLoggerComponent(kLoggerComponent);
  media_route_starter_->AddMediaSinkWithCastModesObserver(this);
  GetMediaRouter()->OnUserGesture();
}

void AccessCodeCastHandler::AddSink(
    const std::string& access_code,
    access_code_cast::mojom::CastDiscoveryMethod discovery_method,
    AddSinkCallback callback) {
  DCHECK(media_route_starter_) << "Must have a MediaRouteStarter";

  AddSinkCallback callback_with_default_invoker =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), AddSinkResultCode::UNKNOWN_ERROR);
  add_sink_callback_ = std::move(base::BindOnce(&AddSinkMetricsCallback))
                           .Then(std::move(callback_with_default_invoker));
  add_sink_request_time_ = base::Time::Now();

  if (!media_route_starter_) {
    std::move(add_sink_callback_).Run(AddSinkResultCode::UNKNOWN_ERROR);
    return;
  }

  if (!IsAccountSyncEnabled()) {
    GetMediaRouter()->GetLogger()->LogError(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "Sync is either pasused or diabled for this account. It must be "
        "enabled fully for the access code casting flow to communicate with "
        "the server.",
        "", "", "");
    std::move(add_sink_callback_).Run(AddSinkResultCode::PROFILE_SYNC_ERROR);
    return;
  }

  access_code_sink_service_->DiscoverSink(
      access_code, base::BindOnce(&AccessCodeCastHandler::OnSinkAddedResult,
                                  weak_ptr_factory_.GetWeakPtr()));
}

bool AccessCodeCastHandler::IsCastModeAvailable(MediaCastMode mode) const {
  return base::Contains(cast_mode_set_, mode);
}

// Discovery is not complete until the sink is in QRM. This is because any
// attempt to create route parameters before the sink is in QRM will fail.
void AccessCodeCastHandler::CheckForDiscoveryCompletion() {
  // Dialog  has already notified (with most likely an error).
  if (!add_sink_callback_)
    return;
  DCHECK(sink_id_) << "Must have a sink id to complete!";
  DCHECK(media_route_starter_) << "Must have a MediaRouteStarter to complete!";

  // Verify that the sink is in QRM.
  if (base::ranges::none_of(cast_mode_set_, [this](MediaCastMode cast_mode) {
        return media_route_starter_->SinkSupportsCastMode(*sink_id_, cast_mode);
      })) {
    // sink hasn't been added to QRM yet.
    return;
  }

  std::move(add_sink_callback_).Run(AddSinkResultCode::OK);
}

void AccessCodeCastHandler::OnSinkAddedResult(
    access_code_cast::mojom::AddSinkResultCode add_sink_result,
    std::optional<MediaSink::Id> sink_id) {
  DCHECK(sink_id || add_sink_result != AddSinkResultCode::OK);

  if (add_sink_result == AddSinkResultCode::ACCESS_CODE_NOT_FOUND)
    access_code_not_found_count_++;

  // Wait for OnResultsUpdated before triggering the |add_sink_callback_| since
  // we are not entirely sure the sink is ready to be casted to yet.
  if (add_sink_result != AddSinkResultCode::OK && add_sink_callback_) {
    auto error_message =
        std::string("The device could not be added because of enum error : ") +
        AddSinkResultCodeToString(add_sink_result);
    GetMediaRouter()->GetLogger()->LogError(
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
    GetMediaRouter()->GetLogger()->LogInfo(
        mojom::LogCategory::kDiscovery, kLoggerComponent,
        "The QueryManager observer has been alerted about the availability of "
        "newly discovered sinks.",
        sink_id_.value(), "", "");
    CheckForDiscoveryCompletion();
  }
}

void AccessCodeCastHandler::CastToSink(CastToSinkCallback callback) {
  DCHECK(media_route_starter_) << "Must have a MediaRouteStarter";
  if (!media_route_starter_) {
    std::move(callback).Run(RouteRequestResultCode::UNKNOWN_ERROR);
    return;
  }

  DCHECK(sink_id_) << "Cast called without a valid sink id!";

  GetMediaRouter()->GetLogger()->LogInfo(
      mojom::LogCategory::kUi, kLoggerComponent,
      "CreateRoute requested by AccessCodeCastHandler.", sink_id_.value(), "",
      "");

  std::unique_ptr<RouteParameters> params;
  MediaCastMode cast_mode;

  // Walk through the available cast modes, and look for one for that matches
  // the available sources. First match wins. params & cast_mode should be set
  // to valid values if this loop completes successfully.
  for (auto it = cast_mode_set_.begin(); it != cast_mode_set_.end() && !params;
       ++it) {
    cast_mode = *it;
    params = media_route_starter_->CreateRouteParameters(*sink_id_, cast_mode);
  }

  if (!params) {
    GetMediaRouter()->GetLogger()->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "No corresponding MediaSource for cast modes " +
            CastModeSetToString(cast_mode_set_),
        sink_id_.value(), "", "");
    std::move(callback).Run(RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  if (!MediaRouteStarter::GetScreenCapturePermission(params->cast_mode)) {
    std::move(callback).Run(RouteRequestResultCode::CANCELLED);
    return;
  }

  current_route_request_ = std::make_optional(*params->request);

  if (HasActiveRoute(sink_id_.value())) {
    GetMediaRouter()->GetLogger()->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "There already exists a route for the given sink id. No new route can "
        "be created. Checking to see if this is a saved device -- otherwise we "
        "wil remove it from the media router.",
        sink_id_.value(), "", "");
    access_code_sink_service_->CheckMediaSinkForExpiration(sink_id_.value());
    std::move(callback).Run(RouteRequestResultCode::ROUTE_ALREADY_EXISTS);
    return;
  }

  params->route_result_callbacks.push_back(base::BindOnce(
      &AccessCodeCastHandler::OnRouteResponse, weak_ptr_factory_.GetWeakPtr(),
      cast_mode, params->request->id, *sink_id_, std::move(callback)));

  media_route_starter_->StartRoute(std::move(params));
}

void AccessCodeCastHandler::OnSinkAddedResultForTesting(
    access_code_cast::mojom::AddSinkResultCode add_sink_result,
    std::optional<MediaSink::Id> sink_id) {
  OnSinkAddedResult(add_sink_result, sink_id);
}

void AccessCodeCastHandler::OnSinksUpdatedForTesting(
    const std::vector<MediaSinkWithCastModes>& sinks) {
  OnSinksUpdated(sinks);
}

// MediaRouter::CreateRoute callback handler - log the success / failure of the
// CreateRoute operation and return the result code to the dialog.
// If there is a presentation request, call the appropriate presentation
// callbacks with the presentation connection.
void AccessCodeCastHandler::OnRouteResponse(MediaCastMode cast_mode,
                                            int route_request_id,
                                            const MediaSink::Id& sink_id,
                                            CastToSinkCallback dialog_callback,
                                            const RouteRequestResult& result) {
  // Only respond to expected responses.
  if (!current_route_request_ ||
      route_request_id != current_route_request_->id) {
    return;
  }
  current_route_request_.reset();

  AccessCodeCastMetrics::OnCastSessionResult(
      static_cast<int>(result.result_code()), CastModeMetricsHelper(cast_mode));

  const MediaRoute* route = result.route();
  if (!route) {
    DCHECK(result.result_code() != mojom::RouteRequestResultCode::OK)
        << "No route but OK response";
    // The provider will handle sending an issue for a failed route request.
    GetMediaRouter()->GetLogger()->LogError(
        mojom::LogCategory::kUi, kLoggerComponent,
        "MediaRouteResponse returned error: " + result.error(), sink_id, "",
        "");
    std::move(dialog_callback).Run(result.result_code());
    return;
  }

  AccessCodeCastMetrics::RecordNewDeviceConnectDuration(base::Time::Now() -
                                                        add_sink_request_time_);
  base::UmaHistogramSparse("MediaRouter.Source.CastingSource", cast_mode);
  std::move(dialog_callback).Run(RouteRequestResultCode::OK);
}

bool AccessCodeCastHandler::HasActiveRoute(const MediaSink::Id& sink_id) {
  return GetMediaRouter() &&
         base::Contains(GetMediaRouter()->GetCurrentRoutes(), sink_id,
                        &MediaRoute::media_sink_id);
}

void AccessCodeCastHandler::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

void AccessCodeCastHandler::SetSyncServiceForTesting(
    syncer::SyncService* sync_service) {
  sync_service_ = sync_service;
}

bool AccessCodeCastHandler::IsAccountSyncEnabled() {
  if (!identity_manager_ || !sync_service_)
    return false;
  return identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
         sync_service_->IsSyncFeatureActive();
}

}  // namespace media_router
