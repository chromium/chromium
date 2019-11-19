// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_DOWNLOAD_PARAMS_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_DOWNLOAD_PARAMS_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "components/download/public/background_service/clients.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace download {

// The parameters describing when to run a download.  This allows the caller to
// specify restrictions on what impact this download will have on the device
// (battery, network conditions, priority, etc.).
struct SchedulingParams {
 public:
  enum class NetworkRequirements {
    // The download can occur under all network conditions.
    NONE = 0,

    // The download should occur when the network isn't metered.  However if the
    // device does not provide that opportunity over a long period of time, the
    // DownloadService may start allowing these downloads to run on metered
    // networks as well.
    OPTIMISTIC = 1,

    // The download can occur only if the network isn't metered.
    UNMETERED = 2,

    // Last value of the enum.
    COUNT = 3,
  };

  enum class BatteryRequirements {
    // The download can occur under all battery scenarios.
    BATTERY_INSENSITIVE = 0,

    // The download can only occur when device is charging or above optimal
    // battery percentage.
    BATTERY_SENSITIVE = 1,

    // Download can only occur when the device is charging.
    BATTERY_CHARGING = 2,

    // Last value of the enum.
    COUNT = 3,
  };

  enum class Priority {
    // The lowest priority.  Requires that the device is idle or Chrome is
    // running. Gets paused or postponed during on-going navigation.
    LOW = 0,

    // The normal priority.  Requires that the device is idle or Chrome is
    // running. Gets paused or postponed during on-going navigation.
    NORMAL = 1,

    // The highest background priority.  Does not require the device to be idle.
    HIGH = 2,

    // The highest priority.  This will act (scheduling requirements aside) as a
    // user-initiated download.
    UI = 3,

    // The default priority for all tasks unless overridden.
    DEFAULT = NORMAL,

    // Last value of the enum.
    COUNT = 4,
  };

  SchedulingParams();
  SchedulingParams(const SchedulingParams& other) = default;
  ~SchedulingParams() = default;

  bool operator==(const SchedulingParams& rhs) const;

  // Cancel the download after this time.  Will cancel in-progress downloads.
  // base::Time::Max() if not specified.
  base::Time cancel_time;

  // The suggested priority.  Non-UI priorities may not be honored by the
  // DownloadService based on internal criteria and settings.
  Priority priority;
  NetworkRequirements network_requirements;
  BatteryRequirements battery_requirements;
};

// The parameters describing how to build the request when starting a download.
struct RequestParams {
 public:
  RequestParams();
  RequestParams(const RequestParams& other);
  ~RequestParams() = default;

  GURL url;

  // The request method ("GET" is the default value).
  std::string method;
  net::HttpRequestHeaders request_headers;

  // If the request will fetch HTTP error response body and treat them as
  // a successful download.
  bool fetch_error_body;

  // Whether the download is not trustworthy and requires safe browsing checks.
  bool require_safety_checks;
};

// The parameters that describe a download request made to the DownloadService.
// The |client| needs to be properly created and registered for this service for
// the download to be accepted.
struct DownloadParams {
  enum StartResult {
    // The download is accepted and persisted.
    ACCEPTED,

    // The DownloadService has too many downloads.  Backoff and retry.
    BACKOFF,

    // The DownloadService has no knowledge of the DownloadClient associated
    // with this request.
    UNEXPECTED_CLIENT,

    // Failed to create the download.  The guid is already in use.
    UNEXPECTED_GUID,

    // The download was cancelled by the Client while it was being persisted.
    CLIENT_CANCELLED,

    // The DownloadService was unable to accept and persist this download due to
    // an internal error like the underlying DB store failing to write to disk.
    INTERNAL_ERROR,

    // TODO(dtrainor): Add more error codes.
    // The count of entries for the enum.
    COUNT,
  };

  using StartCallback = base::Callback<void(const std::string&, StartResult)>;

  DownloadParams();
  DownloadParams(const DownloadParams& other);
  ~DownloadParams();

  // The feature that is requesting this download.
  DownloadClient client;

  // A unique GUID that represents this download.  See |base::GenerateGUID()|.
  std::string guid;

  // A callback that will be notified if this download has been accepted and
  // persisted by the DownloadService.
  StartCallback callback;

  // The parameters that determine under what device conditions this download
  // will occur.
  SchedulingParams scheduling_params;

  // The parameters that define the actual download request to make.
  RequestParams request_params;

  // Traffic annotation for the network request.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_DOWNLOAD_PARAMS_H_
