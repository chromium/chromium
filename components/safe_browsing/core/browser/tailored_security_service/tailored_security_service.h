// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace signin {
class IdentityManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace safe_browsing {

class TailoredSecurityServiceObserver;

// Provides an API for querying Google servers for a user's tailored security
// account Opt-In.
class TailoredSecurityService : public KeyedService {
 public:
  // Handles all the work of making an API request. This class encapsulates
  // the entire state of the request. When an instance is destroyed, all
  // aspects of the request are cancelled.
  class Request {
   public:
    virtual ~Request();

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    // Returns true if the request is "pending" (i.e., it has been started, but
    // is not yet completed).
    virtual bool IsPending() const = 0;

    // Returns the response code received from the server, which will only be
    // valid if the request succeeded.
    virtual int GetResponseCode() const = 0;

    // Returns the contents of the response body received from the server.
    virtual const std::string& GetResponseBody() const = 0;

    virtual void SetPostData(const std::string& post_data) = 0;

    // Tells the request to begin.
    virtual void Start() = 0;

    virtual void Shutdown() = 0;

   protected:
    Request();
  };

  using QueryTailoredSecurityBitCallback =
      base::OnceCallback<void(bool is_enabled)>;

  using CompletionCallback = base::OnceCallback<void(Request*, bool success)>;

  TailoredSecurityService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~TailoredSecurityService() override;

  void AddObserver(TailoredSecurityServiceObserver* observer);
  void RemoveObserver(TailoredSecurityServiceObserver* observer);

  // Queries whether TailoredSecurity is enabled on the server.
  void QueryTailoredSecurityBit();

  // Starts the request to send to the backend to retrieve the bit.
  void StartRequest(QueryTailoredSecurityBitCallback callback,
                    const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Sets the state of tailored security bit to |is_enabled| for testing.
  void SetTailoredSecurityBitForTesting(
      bool is_enabled,
      QueryTailoredSecurityBitCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // KeyedService implementation:
  void Shutdown() override;

 protected:
  // This function is pulled out for testing purposes. Caller takes ownership of
  // the new Request.
  virtual std::unique_ptr<Request> CreateRequest(
      const GURL& url,
      CompletionCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Used for tests.
  size_t GetNumberOfPendingTailoredSecurityServiceRequests();

  // Extracts a JSON-encoded HTTP response into a dictionary.
  static base::Value ReadResponse(Request* request);

  // Called by `request` when a tailored security service query has completed.
  // Unpacks the response and calls `callback`, which is the original callback
  // that was passed to QueryTailoredSecurityBit().
  void QueryTailoredSecurityBitCompletionCallback(
      QueryTailoredSecurityBitCallback callback,
      Request* request,
      bool success);
  void OnTailoredSecurityBitRetrieved(bool is_enabled);

 private:
  // Stores pointer to IdentityManager instance. It must outlive the
  // TailoredSecurityService and can be null during tests.
  signin::IdentityManager* identity_manager_;

  // Request context getter to use.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Pending TailoredSecurity queries to be canceled if not complete by
  // profile shutdown.
  std::map<Request*, std::unique_ptr<Request>>
      pending_tailored_security_requests_;

  // Observers.
  base::ObserverList<TailoredSecurityServiceObserver, true>::Unchecked
      observer_list_;

  // Timer to periodically check tailored security bit.
  base::RepeatingTimer timer_;

  bool is_shut_down_ = false;

  base::WeakPtrFactory<TailoredSecurityService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_H_
