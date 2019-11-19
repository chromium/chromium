// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_
#define CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/net/connectivity_checker.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace chromecast {

// Simple class to check network connectivity by sending a HEAD http request
// to given url.
class ConnectivityCheckerImpl
    : public ConnectivityChecker,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Connectivity checking and initialization will run on task_runner.
  static scoped_refptr<ConnectivityCheckerImpl> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      network::NetworkConnectionTracker* network_connection_tracker);

  // ConnectivityChecker implementation:
  bool Connected() const override;
  void Check() override;

 protected:
  explicit ConnectivityCheckerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      network::NetworkConnectionTracker* network_connection_tracker);
  ~ConnectivityCheckerImpl() override;

 private:
  // Initializes ConnectivityChecker
  void Initialize(std::unique_ptr<network::SharedURLLoaderFactoryInfo>
                      url_loader_factory_info);

  // network::NetworkConnectionTracker::NetworkConnectionObserver
  // implementation:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  void OnConnectionChangedInternal();

  void OnConnectivityCheckComplete(
      scoped_refptr<net::HttpResponseHeaders> headers);

  // Cancels current connectivity checking in progress.
  void Cancel();

  // Sets connectivity and alerts observers if it has changed
  void SetConnected(bool connected);

  enum class ErrorType {
    BAD_HTTP_STATUS = 1,
    SSL_CERTIFICATE_ERROR = 2,
    REQUEST_TIMEOUT = 3,
  };

  // Called when URL request failed.
  void OnUrlRequestError(ErrorType type);

  // Called when URL request timed out.
  void OnUrlRequestTimeout();

  void CheckInternal();

  std::unique_ptr<GURL> connectivity_check_url_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  network::NetworkConnectionTracker* const network_connection_tracker_;

  // connected_lock_ protects access to connected_ which is shared across
  // threads.
  mutable base::Lock connected_lock_;
  bool connected_;

  network::mojom::ConnectionType connection_type_;
  // Number of connectivity check errors.
  unsigned int check_errors_;
  bool network_changed_pending_;
  // Timeout handler for connectivity checks.
  // Note: Cancelling this timeout can cause the destructor for this class to be
  // called.
  base::CancelableCallback<void()> timeout_;

  base::WeakPtr<ConnectivityCheckerImpl> weak_this_;
  base::WeakPtrFactory<ConnectivityCheckerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectivityCheckerImpl);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_CONNECTIVITY_CHECKER_IMPL_H_
