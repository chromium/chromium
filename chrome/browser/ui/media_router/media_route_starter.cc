// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_route_starter.h"

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/issue_manager.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/common/media_source.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "ui/base/cocoa/permissions_utils.h"
#endif

namespace media_router {

namespace {
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

MediaRouteStarter::MediaRouteStarter(
    const CastModeSet& initial_modes,
    content::WebContents* web_contents,
    std::unique_ptr<StartPresentationContext> start_presentation_context)
    : web_contents_(web_contents),
      start_presentation_context_(std::move(start_presentation_context)),
      presentation_manager_(
          web_contents ? WebContentsPresentationManager::Get(web_contents)
                       : nullptr),
      query_result_manager_(
          std::make_unique<QueryResultManager>(GetMediaRouter())) {
  if (presentation_manager_)
    presentation_manager_->AddObserver(this);
  InitPresentationSources(initial_modes);
  InitMirroringSources(initial_modes);
}

MediaRouteStarter::~MediaRouteStarter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (presentation_manager_)
    presentation_manager_->RemoveObserver(this);

  // If |start_presentation_context_| still exists, then it means presentation
  // route request was never attempted.
  if (start_presentation_context_) {
    std::vector<MediaSinkWithCastModes> sinks =
        GetQueryResultManager()->GetSinksWithCastModes();
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

void MediaRouteStarter::AddPresentationRequestSourceObserver(
    PresentationRequestSourceObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void MediaRouteStarter::RemovePresentationRequestSourceObserver(
    PresentationRequestSourceObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void MediaRouteStarter::AddMediaSinkWithCastModesObserver(
    MediaSinkWithCastModesObserver* observer) {
  GetQueryResultManager()->AddObserver(observer);
}

void MediaRouteStarter::RemoveMediaSinkWithCastModesObserver(
    MediaSinkWithCastModesObserver* observer) {
  GetQueryResultManager()->RemoveObserver(observer);
}

Profile* MediaRouteStarter::GetProfile() const {
  return GetWebContents() && GetWebContents()->GetBrowserContext()
             ? Profile::FromBrowserContext(
                   GetWebContents()->GetBrowserContext())
             : ProfileManager::GetActiveUserProfile();
}

MediaRouter* MediaRouteStarter::GetMediaRouter() const {
  return MediaRouterFactory::GetApiForBrowserContext(GetProfile());
}

std::unique_ptr<RouteParameters> MediaRouteStarter::CreateRouteParameters(
    const MediaSink::Id& sink_id,
    MediaCastMode cast_mode) {
  std::unique_ptr<RouteParameters> params = std::make_unique<RouteParameters>();

  params->cast_mode = cast_mode;

  std::unique_ptr<MediaSource> source =
      GetQueryResultManager()->GetSourceForCastModeAndSink(cast_mode, sink_id);
  if (!source) {
    return nullptr;
  }
  params->source_id = source->id();

  bool for_presentation_source = cast_mode == MediaCastMode::PRESENTATION;
  if (for_presentation_source && !presentation_request_) {
    GetMediaRouter()->GetLogger()->LogError(
        mojom::LogCategory::kUi, component_,
        "Requested to create a route for presentation, but "
        "presentation request is missing.",
        sink_id, source->id(), "");
    return nullptr;
  }

  params->request = std::make_unique<RouteRequest>(sink_id);

  params->origin = for_presentation_source ? presentation_request_->frame_origin
                                           : url::Origin::Create(GURL());

  params->timeout = GetRouteRequestTimeout(cast_mode);
  params->off_the_record =
      GetWebContents() &&
      GetWebContents()->GetBrowserContext()->IsOffTheRecord();

  return params;
}

bool MediaRouteStarter::GetScreenCapturePermission(MediaCastMode cast_mode) {
  if (!RequiresScreenCapturePermission(cast_mode))
    return true;

  return media_router::GetScreenCapturePermission();
}

void MediaRouteStarter::StartRoute(std::unique_ptr<RouteParameters> params) {
  DCHECK(params) << "Must have params!";
  DCHECK(params->request) << "Must have params->request!";
  DCHECK(!params->presentation_callback)
      << "params->presentation_callback is not used!";

  MediaRouteResponseCallback presentation_callback;

  if (params->cast_mode == MediaCastMode::PRESENTATION) {
    if (start_presentation_context_) {
      presentation_callback =
          base::BindOnce(&StartPresentationContext::HandleRouteResponse,
                         std::move(start_presentation_context_));
    } else if (presentation_manager_) {
      presentation_callback = base::BindOnce(
          &WebContentsPresentationManager::OnPresentationResponse,
          presentation_manager_, *presentation_request_);
    } else {
      NOTREACHED();
    }
  }

  GetMediaRouter()->CreateRoute(
      params->source_id, params->request->sink_id, params->origin,
      GetWebContents(),
      base::BindOnce(&RunRouteResponseCallbacks,
                     std::move(presentation_callback),
                     std::move(params->route_result_callbacks)),
      params->timeout, params->off_the_record);
}

std::u16string MediaRouteStarter::GetPresentationRequestSourceName() const {
  const url::Origin frame_origin = GetFrameOrigin();
  // Presentation URLs are only possible on https: and other secure contexts,
  // so we can omit http/https schemes here.
  return frame_origin.scheme() == extensions::kExtensionScheme
             ? base::UTF8ToUTF16(GetExtensionName(
                   frame_origin.GetURL(),
                   extensions::ExtensionRegistry::Get(GetBrowserContext())))
             : url_formatter::FormatOriginForSecurityDisplay(
                   frame_origin,
                   url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

void MediaRouteStarter::OnDefaultPresentationChanged(
    const content::PresentationRequest* presentation_request) {
  if (presentation_request) {
    std::vector<MediaSource> sources;
    for (const auto& url : presentation_request->presentation_urls) {
      sources.push_back(MediaSource::ForPresentationUrl(url));
    }
    presentation_request_ = *presentation_request;
    GetQueryResultManager()->SetSourcesForCastMode(
        MediaCastMode::PRESENTATION, sources,
        presentation_request_->frame_origin);
  } else {
    presentation_request_.reset();
    GetQueryResultManager()->RemoveSourcesForCastMode(
        MediaCastMode::PRESENTATION);
  }

  auto name = GetPresentationRequestSourceName();
  for (PresentationRequestSourceObserver& observer : observers_) {
    observer.OnSourceUpdated(name);
  }
}

void MediaRouteStarter::InitPresentationSources(
    const CastModeSet& initial_modes) {
  if (!IsCastModeAvailable(initial_modes, MediaCastMode::PRESENTATION)) {
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

void MediaRouteStarter::InitMirroringSources(const CastModeSet& initial_modes) {
  // Use a placeholder URL as origin for mirroring.
  url::Origin origin = url::Origin::Create(GURL());

  if (IsCastModeAvailable(initial_modes, MediaCastMode::DESKTOP_MIRROR)) {
    GetQueryResultManager()->SetSourcesForCastMode(
        MediaCastMode::DESKTOP_MIRROR, {MediaSource::ForUnchosenDesktop()},
        origin);
  }

  if (IsCastModeAvailable(initial_modes, MediaCastMode::TAB_MIRROR)) {
    SessionID::id_type tab_id =
        sessions::SessionTabHelper::IdForTab(web_contents_).id();
    if (tab_id != -1) {
      MediaSource mirroring_source(MediaSource::ForTab(tab_id));
      GetQueryResultManager()->SetSourcesForCastMode(
          MediaCastMode::TAB_MIRROR, {mirroring_source}, origin);
    }
  }
}

content::BrowserContext* MediaRouteStarter::GetBrowserContext() const {
  return GetWebContents() ? GetWebContents()->GetBrowserContext()
                          : ProfileManager::GetActiveUserProfile();
}

url::Origin MediaRouteStarter::GetFrameOrigin() const {
  return presentation_request_ ? presentation_request_->frame_origin
                               : url::Origin();
}

bool MediaRouteStarter::IsCastModeAvailable(const CastModeSet& modes,
                                            MediaCastMode mode) {
  return base::Contains(modes, mode);
}

}  // namespace media_router
