// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/common/pref_names.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom-shared.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace media_router {

using mojom::MediaRouteProviderId;

namespace {

DialogActivationLocationAndCastMode GetActivationLocationAndCastMode(
    MediaRouterDialogActivationLocation activation_location,
    MediaCastMode cast_mode,
    bool is_icon_pinned) {
  switch (activation_location) {
    case MediaRouterDialogActivationLocation::TOOLBAR:
      if (is_icon_pinned) {
        switch (cast_mode) {
          case MediaCastMode::PRESENTATION:
            return DialogActivationLocationAndCastMode::
                kPinnedIconAndPresentation;
          case MediaCastMode::TAB_MIRROR:
            return DialogActivationLocationAndCastMode::kPinnedIconAndTabMirror;
          case MediaCastMode::DESKTOP_MIRROR:
            return DialogActivationLocationAndCastMode::
                kPinnedIconAndDesktopMirror;
          case MediaCastMode::REMOTE_PLAYBACK:
            return DialogActivationLocationAndCastMode::
                kPinnedIconAndRemotePlayback;
        }
      } else {
        switch (cast_mode) {
          case MediaCastMode::PRESENTATION:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndPresentation;
          case MediaCastMode::TAB_MIRROR:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndTabMirror;
          case MediaCastMode::DESKTOP_MIRROR:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndDesktopMirror;
          case MediaCastMode::REMOTE_PLAYBACK:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndRemotePlayback;
        }
      }
      break;
    case MediaRouterDialogActivationLocation::CONTEXTUAL_MENU:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::
              kContextMenuAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kContextMenuAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::
              kContextMenuAndDesktopMirror;
        case MediaCastMode::REMOTE_PLAYBACK:
          return DialogActivationLocationAndCastMode::
              kContextMenuAndRemotePlayback;
      }
      break;
    case MediaRouterDialogActivationLocation::PAGE:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::kPageAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kPageAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::kPageAndDesktopMirror;
        case MediaCastMode::REMOTE_PLAYBACK:
          return DialogActivationLocationAndCastMode::kPageAndRemotePlayback;
      }
      break;
    case MediaRouterDialogActivationLocation::APP_MENU:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::kAppMenuAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kAppMenuAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::kAppMenuAndDesktopMirror;
        case MediaCastMode::REMOTE_PLAYBACK:
          return DialogActivationLocationAndCastMode::kAppMenuAndRemotePlayback;
      }
      break;
    case MediaRouterDialogActivationLocation::SHARING_HUB:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::
              kSharingHubAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kSharingHubAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::
              kSharingHubAndDesktopMirror;
        case MediaCastMode::REMOTE_PLAYBACK:
          return DialogActivationLocationAndCastMode::
              kSharingHubAndRemotePlayback;
      }
      break;
    // |OVERFLOW_MENU| refers to extension icons hidden in the app menu. That
    // mode is no longer available for the Cast toolbar icon.
    case MediaRouterDialogActivationLocation::OVERFLOW_MENU:
    case MediaRouterDialogActivationLocation::SYSTEM_TRAY:
    case MediaRouterDialogActivationLocation::TOTAL_COUNT:
      break;
  }
  NOTREACHED();
}

}  // namespace

CastDialogMetrics::CastDialogMetrics(
    const base::Time& initialization_time,
    MediaRouterDialogActivationLocation activation_location,
    Profile* profile)
    : initialization_time_(initialization_time),
      activation_location_(activation_location),
      is_icon_pinned_(
          profile->GetPrefs()->GetBoolean(::prefs::kShowCastIconInToolbar)) {
  MediaRouterMetrics::RecordIconStateAtDialogOpen(is_icon_pinned_);
}

CastDialogMetrics::~CastDialogMetrics() = default;

void CastDialogMetrics::OnSinksLoaded(const base::Time& sinks_load_time) {
  if (!sinks_load_time_.is_null())
    return;
  MediaRouterMetrics::RecordCastDialogLoaded(sinks_load_time -
                                             initialization_time_);
  sinks_load_time_ = sinks_load_time;
}

void CastDialogMetrics::OnStartCasting(MediaCastMode cast_mode,
                                       SinkIconType icon_type) {
  MaybeRecordActivationLocationAndCastMode(cast_mode);
  MediaRouterMetrics::RecordMediaSinkTypeForCastDialog(icon_type);
}

void CastDialogMetrics::OnRecordSinkCount(
    const std::vector<CastDialogSinkButton*>& sink_buttons) {
  media_router::MediaRouterMetrics::RecordDeviceCount(sink_buttons.size());
}

void CastDialogMetrics::OnRecordSinkCount(
    const std::vector<raw_ptr<CastDialogSinkView, DanglingUntriaged>>&
        sink_views) {
  media_router::MediaRouterMetrics::RecordDeviceCount(sink_views.size());
}

void CastDialogMetrics::MaybeRecordActivationLocationAndCastMode(
    MediaCastMode cast_mode) {
  if (activation_location_and_cast_mode_recorded_)
    return;
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Ui.Dialog.ActivationLocationAndCastMode",
      GetActivationLocationAndCastMode(activation_location_, cast_mode,
                                       is_icon_pinned_));
  activation_location_and_cast_mode_recorded_ = true;
}

}  // namespace media_router
