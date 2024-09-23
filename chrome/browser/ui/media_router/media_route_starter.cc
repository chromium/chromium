// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_route_starter.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_router_ui_helper.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_message_center/media_notification_util.h"
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
#include "media/remoting/device_capability_checker.h"
#include "net/base/url_util.h"
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
  for (auto& callback : route_result_callbacks)
    std::move(callback).Run(result);
}

// Gets the profile to use for the `MediaRouteStarter` when there is no
// `WebContents` initiator. On ChromeOS, this happens for example when the
// `MediaRouteStarter` is called from the OS system tray.
Profile* GetDefaultProfileForMediaRouteStarter() {
// Use the main profile on ChromeOS. Desktop platforms don't have the concept
// of a "main" profile, so pick the "last used" profile instead.
#if BUILDFLAG(IS_CHROMEOS)
  return ProfileManager::GetActiveUserProfile();
#else
  return ProfileManager::GetLastUsedProfile();
#endif
}

}  // namespace

MediaRouteStarter::MediaRouteStarter(MediaRouterUIParameters params)
    : web_contents_(params.initiator),
      start_presentation_context_(std::move(params.start_presentation_context)),
      presentation_manager_(
          params.initiator
              ? WebContentsPresentationManager::Get(params.initiator)
              : nullptr),
      query_result_manager_(
          std::make_unique<QueryResultManager>(GetMediaRouter())) {
  if (presentation_manager_)
    presentation_manager_->AddObserver(this);
  InitPresentationSources(params.initial_modes);
  InitMirroringSources(params.initial_modes);
  InitRemotePlaybackSources(params.initial_modes, params.video_codec,
                            params.audio_codec);
}

MediaRouteStarter::~MediaRouteStarter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (presentation_manager_)
    presentation_manager_->RemoveObserver(this);

  // If |start_presentation_context_| still exists, then it means presentation
  // route request was never attempted.
  if (start_presentation_context_) {
    bool presentation_sinks_available = base::ranges::any_of(
        GetQueryResultManager()->GetSinksWithCastModes(),
        [](const MediaSinkWithCastModes& sink) {
          return base::Contains(sink.cast_modes, MediaCastMode::PRESENTATION) ||
                 base::Contains(sink.cast_modes,
                                MediaCastMode::REMOTE_PLAYBACK);
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
             : GetDefaultProfileForMediaRouteStarter();
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

  MediaRouteResponseCallback presentation_callback;

  // There are two ways to initialize MediaRouterUI with Presentation cast mode
  // and each method requires different way to propagate route responses back to
  // sites.
  // 1. For StartPresentationContext passed by sites. We should use
  // the StartPresentationContext for presentation response callback.
  // 2. For the default presentation request managed by
  // WebContentsPresentationManager. In this case, we should use
  // WebContentsPresentationManager::OnPresentationResponse.
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
      NOTREACHED_IN_MIGRATION();
    }
  }

  // There are two ways to initialize MediaRouterUI for REMOTE_PLAYBACK cast
  // mode:
  // 1. Remote Playback API prompt() function passes a StartPresentationContext
  // object. We should use the StartPresentationContext for presentation
  // response callback.
  // 2. Media Session items for Media Remoting from the GMC dialog. In this way,
  // the site does not use the Remote Playback API and there's no need to set up
  // presentation callback.
  if (params->cast_mode == MediaCastMode::REMOTE_PLAYBACK &&
      start_presentation_context_) {
    presentation_callback =
        base::BindOnce(&StartPresentationContext::HandleRouteResponse,
                       std::move(start_presentation_context_));
  }

  GetMediaRouter()->CreateRoute(
      params->source_id, params->request->sink_id, params->origin,
      GetWebContents(),
      base::BindOnce(&RunRouteResponseCallbacks,
                     std::move(presentation_callback),
                     std::move(params->route_result_callbacks)),
      params->timeout);
}

std::u16string MediaRouteStarter::GetPresentationRequestSourceName() const {
  const url::Origin frame_origin = GetFrameOrigin();
  // Presentation URLs are only possible on https: and other secure contexts,
  // so we can omit http/https schemes here.
  return frame_origin.scheme() == extensions::kExtensionScheme
             ? base::UTF8ToUTF16(GetExtensionName(
                   frame_origin.GetURL(),
                   extensions::ExtensionRegistry::Get(GetBrowserContext())))
             : media_message_center::GetOriginNameForDisplay(frame_origin);
}

bool MediaRouteStarter::SinkSupportsCastMode(const MediaSink::Id& sink_id,
                                             MediaCastMode cast_mode) const {
  return GetQueryResultManager()
      ->GetSourceForCastModeAndSink(cast_mode, sink_id)
      .get();
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
    auto media_source =
        MediaSource(start_presentation_context_->presentation_request()
                        .presentation_urls[0]);
    if (media_source.IsRemotePlaybackSource()) {
      media_source.AppendTabIdToRemotePlaybackUrlQuery(
          sessions::SessionTabHelper::IdForTab(web_contents_).id());
      GetQueryResultManager()->SetSourcesForCastMode(
          MediaCastMode::REMOTE_PLAYBACK, {media_source},
          url::Origin::Create(GURL()));
    } else {
      OnDefaultPresentationChanged(
          &start_presentation_context_->presentation_request());
    }
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

void MediaRouteStarter::InitRemotePlaybackSources(
    const CastModeSet& initial_modes,
    media::VideoCodec video_codec,
    media::AudioCodec audio_codec) {
  if (!IsCastModeAvailable(initial_modes, MediaCastMode::REMOTE_PLAYBACK)) {
    return;
  }
  DCHECK(video_codec != media::VideoCodec::kUnknown) << "Unknown video codec.";
  DCHECK(audio_codec != media::AudioCodec::kUnknown) << "Unknown audio codec.";

  // Use a placeholder URL as origin for Remote Playback.
  url::Origin origin = url::Origin::Create(GURL());
  SessionID::id_type tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents_).id();
  GetQueryResultManager()->SetSourcesForCastMode(
      MediaCastMode::REMOTE_PLAYBACK,
      {MediaSource::ForRemotePlayback(tab_id, video_codec, audio_codec)},
      origin);
}

content::BrowserContext* MediaRouteStarter::GetBrowserContext() const {
  return GetWebContents() ? GetWebContents()->GetBrowserContext()
                          : GetDefaultProfileForMediaRouteStarter();
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
