// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace media_router {

namespace {

DialogActivationLocationAndCastMode GetActivationLocationAndCastMode(
    MediaRouterDialogOpenOrigin activation_location,
    MediaCastMode cast_mode,
    bool is_icon_pinned) {
  switch (activation_location) {
    case MediaRouterDialogOpenOrigin::TOOLBAR:
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
          case MediaCastMode::LOCAL_FILE:
            return DialogActivationLocationAndCastMode::kPinnedIconAndLocalFile;
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
          case MediaCastMode::LOCAL_FILE:
            return DialogActivationLocationAndCastMode::
                kEphemeralIconAndLocalFile;
        }
      }
      break;
    case MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::
              kContextMenuAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kContextMenuAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::
              kContextMenuAndDesktopMirror;
        case MediaCastMode::LOCAL_FILE:
          return DialogActivationLocationAndCastMode::kContextMenuAndLocalFile;
      }
      break;
    case MediaRouterDialogOpenOrigin::PAGE:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::kPageAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kPageAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::kPageAndDesktopMirror;
        case MediaCastMode::LOCAL_FILE:
          return DialogActivationLocationAndCastMode::kPageAndLocalFile;
      }
      break;
    case MediaRouterDialogOpenOrigin::APP_MENU:
      switch (cast_mode) {
        case MediaCastMode::PRESENTATION:
          return DialogActivationLocationAndCastMode::kAppMenuAndPresentation;
        case MediaCastMode::TAB_MIRROR:
          return DialogActivationLocationAndCastMode::kAppMenuAndTabMirror;
        case MediaCastMode::DESKTOP_MIRROR:
          return DialogActivationLocationAndCastMode::kAppMenuAndDesktopMirror;
        case MediaCastMode::LOCAL_FILE:
          return DialogActivationLocationAndCastMode::kAppMenuAndLocalFile;
      }
      break;
    // |OVERFLOW_MENU| refers to extension icons hidden in the app menu. That
    // mode is no longer available for the Cast toolbar icon.
    case MediaRouterDialogOpenOrigin::OVERFLOW_MENU:
    case MediaRouterDialogOpenOrigin::TOTAL_COUNT:
      break;
  }
  NOTREACHED();
  return DialogActivationLocationAndCastMode::kMaxValue;
}

}  // namespace

CastDialogMetrics::CastDialogMetrics(
    const base::Time& initialization_time,
    MediaRouterDialogOpenOrigin activation_location,
    Profile* profile)
    : initialization_time_(initialization_time),
      activation_location_(activation_location),
      is_icon_pinned_(
          profile->GetPrefs()->GetBoolean(::prefs::kShowCastIconInToolbar)) {
  MediaRouterMetrics::RecordIconStateAtDialogOpen(is_icon_pinned_);
  MediaRouterMetrics::RecordCloudPrefAtDialogOpen(
      profile->GetPrefs()->GetBoolean(prefs::kMediaRouterEnableCloudServices));
}

CastDialogMetrics::~CastDialogMetrics() = default;

void CastDialogMetrics::OnSinksLoaded(const base::Time& sinks_load_time) {
  if (!sinks_load_time_.is_null())
    return;
  MediaRouterMetrics::RecordMediaRouterDialogLoaded(sinks_load_time -
                                                    initialization_time_);
  sinks_load_time_ = sinks_load_time;
}

void CastDialogMetrics::OnPaint(const base::Time& paint_time) {
  if (!paint_time_.is_null())
    return;
  MediaRouterMetrics::RecordMediaRouterDialogPaint(paint_time -
                                                   initialization_time_);
  paint_time_ = paint_time;
}

void CastDialogMetrics::OnStartCasting(const base::Time& start_time,
                                       int selected_sink_index,
                                       MediaCastMode cast_mode) {
  DCHECK(!sinks_load_time_.is_null());
  MediaRouterMetrics::RecordStartRouteDeviceIndex(selected_sink_index);
  if (!first_action_recorded_) {
    MediaRouterMetrics::RecordStartLocalSessionLatency(start_time -
                                                       sinks_load_time_);
  }
  MaybeRecordFirstAction(MediaRouterUserAction::START_LOCAL);
  MaybeRecordActivationLocationAndCastMode(cast_mode);
}

void CastDialogMetrics::OnStopCasting(bool is_local_route) {
  if (is_local_route) {
    MediaRouterMetrics::RecordStopLocalRoute();
    MaybeRecordFirstAction(MediaRouterUserAction::STOP_LOCAL);
  } else {
    MediaRouterMetrics::RecordStopRemoteRoute();
    MaybeRecordFirstAction(MediaRouterUserAction::STOP_REMOTE);
  }
}

void CastDialogMetrics::OnCastModeSelected() {
  MaybeRecordFirstAction(MediaRouterUserAction::CHANGE_MODE);
}

void CastDialogMetrics::OnCloseDialog(const base::Time& close_time) {
  if (!first_action_recorded_ && !paint_time_.is_null())
    MediaRouterMetrics::RecordCloseDialogLatency(close_time - paint_time_);
  MaybeRecordFirstAction(MediaRouterUserAction::CLOSE);
}

void CastDialogMetrics::OnRecordSinkCount(int sink_count) {
  media_router::MediaRouterMetrics::RecordDeviceCount(sink_count);
}

void CastDialogMetrics::MaybeRecordFirstAction(MediaRouterUserAction action) {
  if (first_action_recorded_)
    return;
  MediaRouterMetrics::RecordMediaRouterInitialUserAction(action);
  first_action_recorded_ = true;
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
