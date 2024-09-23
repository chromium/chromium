// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_metrics.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace media_router {

namespace {

constexpr char kHistogramProviderCreateRouteResult[] =
    "MediaRouter.Provider.CreateRoute.Result";
constexpr char kHistogramProviderJoinRouteResult[] =
    "MediaRouter.Provider.JoinRoute.Result";
constexpr char kHistogramProviderTerminateRouteResult[] =
    "MediaRouter.Provider.TerminateRoute.Result";

std::string GetHistogramNameForProvider(
    const std::string& base_name,
    std::optional<mojom::MediaRouteProviderId> provider_id) {
  if (!provider_id) {
    return base_name;
  }
  switch (*provider_id) {
    case mojom::MediaRouteProviderId::CAST:
      return base_name + ".Cast";
    case mojom::MediaRouteProviderId::DIAL:
      return base_name + ".DIAL";
    case mojom::MediaRouteProviderId::WIRED_DISPLAY:
      return base_name + ".WiredDisplay";
    case mojom::MediaRouteProviderId::ANDROID_CAF:
      return base_name + ".AndroidCaf";
    // The rest use the base histogram name.
    case mojom::MediaRouteProviderId::TEST:
      return base_name;
  }
}

PresentationUrlType GetPresentationUrlType(const GURL& url) {
  if (url.SchemeIs(kDialPresentationUrlScheme))
    return PresentationUrlType::kDial;
  if (url.SchemeIs(kCastPresentationUrlScheme))
    return PresentationUrlType::kCast;
  if (url.SchemeIs(kCastDialPresentationUrlScheme))
    return PresentationUrlType::kCastDial;
  if (url.SchemeIs(kRemotePlaybackPresentationUrlScheme))
    return PresentationUrlType::kRemotePlayback;
  if (base::StartsWith(url.spec(), kLegacyCastPresentationUrlPrefix,
                       base::CompareCase::INSENSITIVE_ASCII))
    return PresentationUrlType::kCastLegacy;
  if (url.SchemeIs(url::kHttpsScheme))
    return PresentationUrlType::kHttps;
  if (url.SchemeIs(url::kHttpScheme))
    return PresentationUrlType::kHttp;
  return PresentationUrlType::kOther;
}

}  // namespace

MediaRouterMetrics::MediaRouterMetrics() = default;
MediaRouterMetrics::~MediaRouterMetrics() = default;

// static
const char MediaRouterMetrics::kHistogramIconClickLocation[] =
    "MediaRouter.Icon.Click.Location";
const char MediaRouterMetrics::kHistogramMediaRouterFileFormat[] =
    "MediaRouter.Source.LocalFileFormat";
const char MediaRouterMetrics::kHistogramMediaRouterFileSize[] =
    "MediaRouter.Source.LocalFileSize";
const char MediaRouterMetrics::kHistogramMediaSinkType[] =
    "MediaRouter.Sink.SelectedType";
const char MediaRouterMetrics::kHistogramPresentationUrlType[] =
    "MediaRouter.PresentationRequest.AvailabilityUrlType";
const char MediaRouterMetrics::kHistogramUiDeviceCount[] =
    "MediaRouter.Ui.Device.Count";
const char MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen[] =
    "MediaRouter.Ui.Dialog.IconStateAtOpen";
const char MediaRouterMetrics::kHistogramUiCastDialogLoadedWithData[] =
    "MediaRouter.Ui.CastDialog.LoadedWithData";
const char MediaRouterMetrics::kHistogramUiGmcDialogLoadedWithData[] =
    "MediaRouter.Ui.GMCDialog.LoadedWithData";
const char MediaRouterMetrics::kHistogramUiAndroidDialogType[] =
    "MediaRouter.Ui.Android.DialogType";
const char MediaRouterMetrics::kHistogramUiAndroidDialogAction[] =
    "MediaRouter.Ui.Android.DialogAction";
const char MediaRouterMetrics::kHistogramUiPermissionRejectedViewAction[] =
    "MediaRouter.Ui.PermissionRejectedViewAction";
const char MediaRouterMetrics::kHistogramUserPromptWhenLaunchingCast[] =
    "MediaRouter.Cast.UserPromptWhenLaunchingCast";
const char MediaRouterMetrics::kHistogramPendingUserAuthLatency[] =
    "MediaRouter.Cast.PendingUserAuthLatency";

// static
const base::TimeDelta MediaRouterMetrics::kDeviceCountMetricDelay =
    base::Seconds(3);

// static
void MediaRouterMetrics::RecordMediaRouterDialogActivationLocation(
    MediaRouterDialogActivationLocation activation_location) {
  base::UmaHistogramEnumeration(
      kHistogramIconClickLocation, activation_location,
      MediaRouterDialogActivationLocation::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordCastDialogLoaded(const base::TimeDelta& delta) {
  base::UmaHistogramTimes(kHistogramUiCastDialogLoadedWithData, delta);
}
// static
void MediaRouterMetrics::RecordGmcDialogLoaded(const base::TimeDelta& delta) {
  base::UmaHistogramTimes(kHistogramUiGmcDialogLoadedWithData, delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterFileFormat(
    const media::container_names::MediaContainerName format) {
  base::UmaHistogramEnumeration(
      kHistogramMediaRouterFileFormat, format);
}

// static
void MediaRouterMetrics::RecordMediaRouterFileSize(int64_t size) {
  base::UmaHistogramMemoryLargeMB(kHistogramMediaRouterFileSize, size);
}

// static
void MediaRouterMetrics::RecordPresentationUrlType(const GURL& url) {
  PresentationUrlType type = GetPresentationUrlType(url);
  base::UmaHistogramEnumeration(kHistogramPresentationUrlType, type,
                                PresentationUrlType::kPresentationUrlTypeCount);
}

// static
void MediaRouterMetrics::RecordMediaSinkType(SinkIconType sink_icon_type) {
  base::UmaHistogramEnumeration(kHistogramMediaSinkType, sink_icon_type,
                                SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordMediaSinkTypeForGlobalMediaControls(
    SinkIconType sink_icon_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramMediaSinkType, ".GlobalMediaControls"}),
      sink_icon_type, SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordMediaSinkTypeForCastDialog(
    SinkIconType sink_icon_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({kHistogramMediaSinkType, ".CastHarmony"}), sink_icon_type,
      SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordDeviceCount(int device_count) {
  base::UmaHistogramCounts100(kHistogramUiDeviceCount, device_count);
}

// static
void MediaRouterMetrics::RecordIconStateAtDialogOpen(bool is_pinned) {
  base::UmaHistogramBoolean(kHistogramUiDialogIconStateAtOpen, is_pinned);
}

// static
void MediaRouterMetrics::RecordCreateRouteResultCode(
    mojom::RouteRequestResultCode result_code,
    std::optional<mojom::MediaRouteProviderId> provider_id) {
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderCreateRouteResult,
                                  provider_id),
      result_code);
}

// static
void MediaRouterMetrics::RecordJoinRouteResultCode(
    mojom::RouteRequestResultCode result_code,
    std::optional<mojom::MediaRouteProviderId> provider_id) {
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderJoinRouteResult,
                                  provider_id),
      result_code);
}

// static
void MediaRouterMetrics::RecordMediaRouteProviderTerminateRoute(
    mojom::RouteRequestResultCode result_code,
    std::optional<mojom::MediaRouteProviderId> provider_id) {
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderTerminateRouteResult,
                                  provider_id),
      result_code);
}

// static
void MediaRouterMetrics::RecordMediaRouterAndroidDialogType(
    MediaRouterAndroidDialogType type) {
  base::UmaHistogramEnumeration(kHistogramUiAndroidDialogType, type);
}

// static
void MediaRouterMetrics::RecordMediaRouterAndroidDialogAction(
    MediaRouterAndroidDialogAction action) {
  base::UmaHistogramEnumeration(kHistogramUiAndroidDialogAction, action);
}

// static
void MediaRouterMetrics::RecordMediaRouterUserPromptWhenLaunchingCast(
    MediaRouterUserPromptWhenLaunchingCast user_prompt) {
  base::UmaHistogramEnumeration(kHistogramUserPromptWhenLaunchingCast,
                                user_prompt);
}

// static
void MediaRouterMetrics::RecordMediaRouterPendingUserAuthLatency(
    const base::TimeDelta& delta) {
  base::UmaHistogramTimes(kHistogramPendingUserAuthLatency, delta);
}

void MediaRouterMetrics::RecordMediaRouterUiPermissionRejectedViewEvents(
    MediaRouterUiPermissionRejectedViewEvents event) {
  base::UmaHistogramEnumeration(kHistogramUiPermissionRejectedViewAction,
                                event);
}

}  // namespace media_router
