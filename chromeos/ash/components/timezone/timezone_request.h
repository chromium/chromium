// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_REQUEST_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace ash {

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_TIMEZONE) TimeZoneResponseData {
  enum Status {
    OK,
    INVALID_REQUEST,
    OVER_QUERY_LIMIT,
    REQUEST_DENIED,
    UNKNOWN_ERROR,
    ZERO_RESULTS,
    REQUEST_ERROR  // local problem
  };

  TimeZoneResponseData();

  std::string ToStringForDebug() const;

  double dstOffset;
  double rawOffset;
  std::string timeZoneId;
  std::string timeZoneName;
  std::string error_message;
  Status status;
};

// Returns default timezone service URL.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_TIMEZONE)
GURL DefaultTimezoneProviderURL();

// Takes Geoposition and sends it to a server to get local timezone information.
// It performs formatting of the request and interpretation of the response.
// If error occurs, request is retried until timeout.
// Zero timeout indicates single request.
// Request is owned and destroyed by caller (usually TimeZoneProvider).
// If request is destroyed while callback has not beed called yet, request
// is silently cancelled.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_TIMEZONE) TimeZoneRequest {
 public:
  // Called when a new geo timezone information is available.
  // The second argument indicates whether there was a server error or not.
  // It is true when there was a server or network error - either no response
  // or a 500 error code.
  using TimeZoneResponseCallback =
      base::OnceCallback<void(std::unique_ptr<TimeZoneResponseData> timezone,
                              bool server_error)>;

  // |url| is the server address to which the request wil be sent.
  // |geoposition| is the location to query timezone for.
  // |retry_timeout| retry request on error until timeout.
  TimeZoneRequest(scoped_refptr<network::SharedURLLoaderFactory> factory,
                  const GURL& service_url,
                  const Geoposition& geoposition,
                  base::TimeDelta retry_timeout);

  TimeZoneRequest(const TimeZoneRequest&) = delete;
  TimeZoneRequest& operator=(const TimeZoneRequest&) = delete;

  ~TimeZoneRequest();

  // Initiates request.
  // Note: if request object is destroyed before callback is called,
  // request will be silently cancelled.
  void MakeRequest(TimeZoneResponseCallback callback);

  void set_retry_sleep_on_server_error_for_testing(
      const base::TimeDelta value) {
    retry_sleep_on_server_error_ = value;
  }

  void set_retry_sleep_on_bad_response_for_testing(
      const base::TimeDelta value) {
    retry_sleep_on_bad_response_ = value;
  }

 private:
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Start new request.
  void StartRequest();

  // Schedules retry.
  void Retry(bool server_error);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  const GURL service_url_;
  Geoposition geoposition_;

  TimeZoneResponseCallback callback_;

  GURL request_url_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // When request was actually started.
  base::Time request_started_at_;

  // Absolute time, when it is passed no more retry requests are allowed.
  base::Time retry_timeout_abs_;

  // Pending retry.
  base::OneShotTimer timezone_request_scheduled_;

  base::TimeDelta retry_sleep_on_server_error_;

  base::TimeDelta retry_sleep_on_bad_response_;

  // Number of retry attempts.
  unsigned retries_;

  // Creation and destruction should happen on the same thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_REQUEST_H_
