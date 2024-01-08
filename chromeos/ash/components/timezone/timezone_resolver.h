// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_RESOLVER_H_
#define CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_RESOLVER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

struct TimeZoneResponseData;

// This class implements periodic timezone synchronization.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_TIMEZONE) TimeZoneResolver {
 public:
  class TimeZoneResolverImpl;

  // This callback will be called when new timezone arrives.
  using ApplyTimeZoneCallback =
      base::RepeatingCallback<void(const TimeZoneResponseData*)>;

  // `ash::DelayNetworkCall` cannot be used directly due to link restrictions.
  using DelayNetworkCallClosure =
      base::RepeatingCallback<void(base::OnceClosure)>;

  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    // Returns true if TimeZoneResolver should include WiFi data in request.
    virtual bool ShouldSendWiFiGeolocationData() const = 0;

    // Returns true if TimeZoneResolver should include Cellular data in request.
    virtual bool ShouldSendCellularGeolocationData() const = 0;
  };

  // This is a LocalState preference to store base::Time value of the last
  // request. It is used to limit request rate on browser restart.
  static const char kLastTimeZoneRefreshTime[];

  TimeZoneResolver(Delegate* delegate,
                   SimpleGeolocationProvider* geolocation_provider_,
                   scoped_refptr<network::SharedURLLoaderFactory> factory,
                   const ApplyTimeZoneCallback& apply_timezone,
                   const DelayNetworkCallClosure& delay_network_call,
                   PrefService* local_state);

  TimeZoneResolver(const TimeZoneResolver&) = delete;
  TimeZoneResolver& operator=(const TimeZoneResolver&) = delete;

  ~TimeZoneResolver();

  // Starts periodic timezone refresh.
  void Start();

  // Cancels current request and stops periodic timezone refresh.
  void Stop();

  // Return true if the periodic timezone scheduler is running (Stop() not
  // called).
  bool IsRunning();

  // Register prefs to LocalState.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const;

  DelayNetworkCallClosure delay_network_call() const {
    return delay_network_call_;
  }

  ApplyTimeZoneCallback apply_timezone() const { return apply_timezone_; }

  PrefService* local_state() const { return local_state_; }

  // Proxy call to Delegate::ShouldSendWiFiGeolocationData().
  bool ShouldSendWiFiGeolocationData() const;

  // Proxy call to Delegate::ShouldSendCellularGeolocationData().
  bool ShouldSendCellularGeolocationData() const;

  // Expose internal fuctions for testing.
  static int MaxRequestsCountForIntervalForTesting(
      const double interval_seconds);
  static int IntervalForNextRequestForTesting(const int requests);

 private:
  bool is_running_ = false;
  const raw_ptr<const Delegate> delegate_;

  // Points to the `SimpleGeolocationProvider::GetInstance()` throughout the
  // object lifecycle. Overridden in unit tests.
  raw_ptr<SimpleGeolocationProvider> geolocation_provider_ = nullptr;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  const ApplyTimeZoneCallback apply_timezone_;
  const DelayNetworkCallClosure delay_network_call_;
  raw_ptr<PrefService> local_state_;

  std::unique_ptr<TimeZoneResolverImpl> implementation_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TIMEZONE_TIMEZONE_RESOLVER_H_
