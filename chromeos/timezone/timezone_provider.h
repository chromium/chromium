// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TIMEZONE_TIMEZONE_PROVIDER_H_
#define CHROMEOS_TIMEZONE_TIMEZONE_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromeos/timezone/timezone_request.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace chromeos {

struct Geoposition;

// This class implements Google TimeZone API.
//
// Note: this should probably be a singleton to monitor requests rate.
// But as it is used only from WizardController, it can be owned by it for now.
class COMPONENT_EXPORT(CHROMEOS_TIMEZONE) TimeZoneProvider {
 public:
  TimeZoneProvider(scoped_refptr<network::SharedURLLoaderFactory> factory,
                   const GURL& url);

  TimeZoneProvider(const TimeZoneProvider&) = delete;
  TimeZoneProvider& operator=(const TimeZoneProvider&) = delete;

  virtual ~TimeZoneProvider();

  // Initiates new request (See TimeZoneRequest for parameters description.)
  void RequestTimezone(const Geoposition& position,
                       base::TimeDelta timeout,
                       TimeZoneRequest::TimeZoneResponseCallback callback);

 private:
  friend class TestTimeZoneAPILoaderFactory;

  // Deletes request from requests_.
  void OnTimezoneResponse(TimeZoneRequest* request,
                          TimeZoneRequest::TimeZoneResponseCallback callback,
                          std::unique_ptr<TimeZoneResponseData> timezone,
                          bool server_error);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  const GURL url_;

  // Requests in progress.
  // TimeZoneProvider owns all requests, so this vector is deleted on destroy.
  std::vector<std::unique_ptr<TimeZoneRequest>> requests_;

  // Creation and destruction should happen on the same thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::TimeZoneProvider;
}

#endif  // CHROMEOS_TIMEZONE_TIMEZONE_PROVIDER_H_
