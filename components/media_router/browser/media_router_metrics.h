// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_METRICS_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_METRICS_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_source.h"
#include "media/base/container_names.h"

class GURL;

namespace media_router {

enum class SinkIconType;

// NOTE: Do not renumber enums as that would confuse interpretation of
// previously logged data. When making changes, also update the enum list
// in tools/metrics/histograms/enums.xml to keep it in sync.

// NOTE: For metrics specific to the Media Router component extension, see
// mojo/media_router_mojo_metrics.h.

// This enum is a cartesian product of dialog activation locations and Cast
// modes. Per tools/metrics/histograms/README.md, a multidimensional histogram
// must be flattened into one dimension.
enum class DialogActivationLocationAndCastMode {
  kPinnedIconAndPresentation,
  kPinnedIconAndTabMirror,
  kPinnedIconAndDesktopMirror,
  kPinnedIconAndLocalFile,
  // One can start casting from an ephemeral icon by stopping a session, then
  // starting another from the same dialog.
  kEphemeralIconAndPresentation,
  kEphemeralIconAndTabMirror,
  kEphemeralIconAndDesktopMirror,
  kEphemeralIconAndLocalFile,
  kContextMenuAndPresentation,
  kContextMenuAndTabMirror,
  kContextMenuAndDesktopMirror,
  kContextMenuAndLocalFile,
  kPageAndPresentation,
  kPageAndTabMirror,
  kPageAndDesktopMirror,
  kPageAndLocalFile,
  kAppMenuAndPresentation,
  kAppMenuAndTabMirror,
  kAppMenuAndDesktopMirror,
  kAppMenuAndLocalFile,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kAppMenuAndLocalFile
};

// Where the user clicked to open the Media Router dialog.
// TODO(takumif): Rename this to DialogActivationLocation to avoid confusing
// "origin" with URL origins.
enum class MediaRouterDialogOpenOrigin {
  TOOLBAR = 0,
  OVERFLOW_MENU = 1,
  CONTEXTUAL_MENU = 2,
  PAGE = 3,
  APP_MENU = 4,

  // NOTE: Add entries only immediately above this line.
  TOTAL_COUNT = 5
};

// The possible outcomes from a route creation response.
enum class MediaRouterRouteCreationOutcome {
  SUCCESS = 0,
  FAILURE_NO_ROUTE = 1,
  FAILURE_INVALID_SINK = 2,

  // Note: Add entries only immediately above this line.
  TOTAL_COUNT = 3,
};

// The possible actions a user can take while interacting with the Media Router
// dialog.
enum class MediaRouterUserAction {
  CHANGE_MODE = 0,
  START_LOCAL = 1,
  STOP_LOCAL = 2,
  CLOSE = 3,
  STATUS_REMOTE = 4,
  REPLACE_LOCAL_ROUTE = 5,
  STOP_REMOTE = 6,

  // Note: Add entries only immediately above this line.
  TOTAL_COUNT = 7
};

enum class PresentationUrlType {
  kOther,
  kCast,            // cast:
  kCastDial,        // cast-dial:
  kCastLegacy,      // URLs that start with |kLegacyCastPresentationUrlPrefix|.
  kDial,            // dial:
  kHttp,            // http:
  kHttps,           // https:
  kRemotePlayback,  // remote-playback:
  // Add new types only immediately above this line. Remember to also update
  // tools/metrics/histograms/enums.xml.
  kPresentationUrlTypeCount
};

class MediaRouterMetrics {
 public:
  MediaRouterMetrics();
  ~MediaRouterMetrics();

  // UMA histogram names.
  static const char kHistogramCloseLatency[];
  static const char kHistogramCloudPrefAtDialogOpen[];
  static const char kHistogramCloudPrefAtInit[];
  static const char kHistogramIconClickLocation[];
  static const char kHistogramMediaRouterFileFormat[];
  static const char kHistogramMediaRouterFileSize[];
  static const char kHistogramMediaSinkType[];
  static const char kHistogramPresentationUrlType[];
  static const char kHistogramRouteCreationOutcome[];
  static const char kHistogramStartLocalLatency[];
  static const char kHistogramStartLocalPosition[];
  static const char kHistogramStartLocalSessionSuccessful[];
  static const char kHistogramStopRoute[];
  static const char kHistogramUiDeviceCount[];
  static const char kHistogramUiDialogActivationLocationAndCastMode[];
  static const char kHistogramUiDialogIconStateAtOpen[];
  static const char kHistogramUiDialogLoadedWithData[];
  static const char kHistogramUiDialogPaint[];
  static const char kHistogramUiFirstAction[];
  static const char kHistogramUiIconStateAtInit[];

  // Records where the user clicked to open the Media Router dialog.
  static void RecordMediaRouterDialogOrigin(MediaRouterDialogOpenOrigin origin);

  // Records the duration it takes for the Media Router dialog to open and
  // finish painting after a user clicks to open the dialog.
  static void RecordMediaRouterDialogPaint(const base::TimeDelta& delta);

  // Records the duration it takes for the Media Router dialog to load its
  // initial data after a user clicks to open the dialog.
  static void RecordMediaRouterDialogLoaded(const base::TimeDelta& delta);

  // Records the duration it takes from the user opening the Media Router dialog
  // to the user closing the dialog. This is only called if closing the dialog
  // is the first action the user takes.
  static void RecordCloseDialogLatency(const base::TimeDelta& delta);

  // Records the first action the user took after the Media Router dialog
  // opened.
  static void RecordMediaRouterInitialUserAction(MediaRouterUserAction action);

  // Records the outcome in a create route response.
  static void RecordRouteCreationOutcome(
      MediaRouterRouteCreationOutcome outcome);

  // Records the format of a cast file.
  static void RecordMediaRouterFileFormat(
      media::container_names::MediaContainerName format);

  // Records the size of a cast file.
  static void RecordMediaRouterFileSize(int64_t size);

  // Records the type of Presentation URL used by a web page.
  static void RecordPresentationUrlType(const GURL& url);

  // Records the type of the sink that media is being Cast to.
  static void RecordMediaSinkType(SinkIconType sink_icon_type);

  // Records the number of devices shown in the Cast dialog. The device count
  // may be 0.
  static void RecordDeviceCount(int device_count);

  // Records the index of the device the user has started casting to on the
  // devices list. The index starts at 0.
  static void RecordStartRouteDeviceIndex(int index);

  // Records the time it takes from the Media Router dialog showing at least one
  // device to the user starting to cast. This is called only if casting is the
  // first action taken by the user, aside from selecting the sink to cast to.
  static void RecordStartLocalSessionLatency(const base::TimeDelta& delta);

  // Records whether or not an attempt to start casting was successful.
  static void RecordStartLocalSessionSuccessful(bool success);

  // Records the user stopping a route in the UI.
  static void RecordStopLocalRoute();
  static void RecordStopRemoteRoute();

  // Records whether the toolbar icon is pinned by the user pref / admin policy.
  // Recorded whenever the Cast dialog is opened.
  static void RecordIconStateAtDialogOpen(bool is_pinned);

  // Records whether the toolbar icon is pinned by the user pref / admin policy.
  // Recorded whenever the browser is initialized.
  static void RecordIconStateAtInit(bool is_pinned);

  // Records the pref value to enable the cloud services. Recorded whenever the
  // Cast dialog is opened.
  static void RecordCloudPrefAtDialogOpen(bool enabled);

  // Records the pref value to enable the cloud services. Recorded whenever the
  // browser is initialized.
  static void RecordCloudPrefAtInit(bool enabled);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_METRICS_H_
