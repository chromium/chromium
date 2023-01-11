// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFETY_CHECK_UPDATE_CHECK_HELPER_H_
#define COMPONENTS_SAFETY_CHECK_UPDATE_CHECK_HELPER_H_

#include "base/functional/callback_forward.h"
#include "net/http/http_response_headers.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safety_check {

// A helper for the update check part of the Safety check to fix missing
// |VersionUpdater| states, such as "Offline" on Windows and Mac.
class UpdateCheckHelper {
 public:
  explicit UpdateCheckHelper(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~UpdateCheckHelper();

  using ConnectivityCheckCallback = base::OnceCallback<void(bool connected)>;

  // Checks connectivity to a Google endpoint (gstatic.com) and invokes the
  // callback with the result. Has a request timeout of 5 seconds. Anything
  // other than the intended HTTP 204 response is considered as no connectivity
  // (user behind proxy, etc).
  virtual void CheckConnectivity(
      ConnectivityCheckCallback connection_check_callback);

 protected:
  // Test-only constructor.
  UpdateCheckHelper();

 private:
  void OnURLLoadComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  ConnectivityCheckCallback result_callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace safety_check

#endif  // COMPONENTS_SAFETY_CHECK_UPDATE_CHECK_HELPER_H_
