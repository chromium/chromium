// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/location_provider.h"
#include "chromeos/ash/components/network/network_util.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class SimpleGeolocationRequestTestMonitor;

// Sends request to a server to get local geolocation information.
// It performs formatting of the request and interpretation of the response.
// Request is owned and destroyed by caller (usually SystemLocationProvider).
// - If error occurs, request is retried until timeout.
// - On successul response, callback is called.
// - On timeout, callback with last (failed) position is called.
// (position.status is set to STATUS_TIMEOUT.)
// - If request is destroyed while callback has not beed called yet, request
// is silently cancelled.
//
// Note: we need COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) for
// tests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    SimpleGeolocationRequest {
 public:
  // |url| is the server address to which the request wil be sent.
  // |timeout| retry request on error until timeout.
  // If wifi_data is not null, it will be sent to the geolocation server.
  // If cell_tower_data is not null, it will be sent to the geolocation server.
  SimpleGeolocationRequest(
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      const GURL& service_url,
      base::TimeDelta timeout,
      std::unique_ptr<WifiAccessPointVector> wifi_data,
      std::unique_ptr<CellTowerVector> cell_tower_data);

  SimpleGeolocationRequest(const SimpleGeolocationRequest&) = delete;
  SimpleGeolocationRequest& operator=(const SimpleGeolocationRequest&) = delete;

  ~SimpleGeolocationRequest();

  // Initiates request.
  // Note: if request object is destroyed before callback is called,
  // request will be silently cancelled.
  void MakeRequest(LocationProvider::ResponseCallback callback);

  void set_retry_sleep_on_server_error_for_testing(
      const base::TimeDelta value) {
    retry_sleep_on_server_error_ = value;
  }

  void set_retry_sleep_on_bad_response_for_testing(
      const base::TimeDelta value) {
    retry_sleep_on_bad_response_ = value;
  }

  // Sets global requests monitoring object for testing.
  static void SetTestMonitor(SimpleGeolocationRequestTestMonitor* monitor);

  std::string FormatRequestBodyForTesting() const;
  GURL GetServiceURLForTesting() const;

 private:
  void OnSimpleURLLoaderComplete(std::optional<std::string> response_body);

  // Start new request.
  void StartRequest();

  // Schedules retry.
  void Retry(bool server_error);

  // Run callback and destroy "this".
  void ReplyAndDestroySelf(const base::TimeDelta elapsed, bool server_error);

  // Called by timeout_timer_ .
  void OnTimeout();

  // Returns API request body.
  std::string FormatRequestBody() const;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Service URL from constructor arguments.
  const GURL service_url_;

  LocationProvider::ResponseCallback callback_;

  // Actual URL with parameters.
  GURL request_url_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // When request was actually started.
  base::Time request_started_at_;

  base::TimeDelta retry_sleep_on_server_error_;

  base::TimeDelta retry_sleep_on_bad_response_;

  const base::TimeDelta timeout_;

  // Pending retry.
  base::OneShotTimer request_scheduled_;

  // Stop request on timeout.
  base::OneShotTimer timeout_timer_;

  // Number of retry attempts.
  unsigned retries_;

  // This is updated on each retry.
  Geoposition position_;

  std::unique_ptr<WifiAccessPointVector> wifi_data_;
  std::unique_ptr<CellTowerVector> cell_tower_data_;

  // Creation and destruction should happen on the same thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_SIMPLE_GEOLOCATION_REQUEST_H_
