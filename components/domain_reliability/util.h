// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_
#define COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/uploader.h"
#include "net/http/http_connection_info.h"

namespace base {
class Location;
}

namespace domain_reliability {

// Attempts to convert a net error and an HTTP response code into the status
// string that should be recorded in a beacon. Returns true if it could.
//
// N.B.: This functions as the whitelist of "safe" errors to report; network-
//       local errors are purposefully not converted to avoid revealing
//       information about the local network to the remote server.
bool GetDomainReliabilityBeaconStatus(
    int net_error,
    int http_response_code,
    std::string* beacon_status_out);

std::string GetDomainReliabilityProtocol(
    net::HttpConnectionInfo connection_info,
    bool ssl_info_populated);

// Based on the network error code, HTTP response code, and Retry-After value,
// returns the result of a report upload.
DomainReliabilityUploader::UploadResult GetUploadResultFromResponseDetails(
    int net_error,
    int http_response_code,
    base::TimeDelta retry_after);

GURL SanitizeURLForReport(
    const GURL& beacon_url,
    const GURL& collector_url,
    const std::vector<std::unique_ptr<std::string>>& path_prefixes);

// Mockable wrapper around TimeTicks::Now and Timer. Mock version is in
// test_util.h.
// TODO(juliatuttle): Rename to Time{Provider,Source,?}.
class DOMAIN_RELIABILITY_EXPORT MockableTime {
 public:
  // Mockable wrapper around (a subset of) base::Timer.
  class DOMAIN_RELIABILITY_EXPORT Timer {
   public:
    virtual ~Timer();

    virtual void Start(const base::Location& posted_from,
                       base::TimeDelta delay,
                       base::OnceClosure user_task) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() = 0;

   protected:
    Timer();
  };

  MockableTime(const MockableTime&) = delete;
  MockableTime& operator=(const MockableTime&) = delete;

  virtual ~MockableTime();

  virtual base::Time Now() const = 0;
  virtual base::TimeTicks NowTicks() const = 0;

  // Returns a new Timer, or a mocked version thereof.
  virtual std::unique_ptr<MockableTime::Timer> CreateTimer() = 0;

  virtual const base::TickClock* AsTickClock() const = 0;

 protected:
  MockableTime();
};

// Implementation of MockableTime that passes through to
// base::Time{,Ticks}::Now() and base::Timer.
class DOMAIN_RELIABILITY_EXPORT ActualTime : public MockableTime {
 public:
  ActualTime();

  ~ActualTime() override;

  // MockableTime implementation:
  base::Time Now() const override;
  base::TimeTicks NowTicks() const override;
  std::unique_ptr<MockableTime::Timer> CreateTimer() override;
  const base::TickClock* AsTickClock() const override;
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_
