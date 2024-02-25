// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_WELL_KNOWN_CHANGE_PASSWORD_STATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_WELL_KNOWN_CHANGE_PASSWORD_STATE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

// A delegate that is notified when the processing is done and its known if
// .well-known/change-password is supported.
class WellKnownChangePasswordStateDelegate {
 public:
  virtual ~WellKnownChangePasswordStateDelegate() = default;
  virtual void OnProcessingFinished(bool is_supported) = 0;
};

// Processes if .well-known/change-password is supported by a site.
class WellKnownChangePasswordState {
 public:
  // Time to wait for the callback from AffiliationService before finishing
  // processing. A callback signals the prefetch action was completed regardless
  // if the response arrived or not.
  static constexpr base::TimeDelta kPrefetchTimeout = base::Seconds(2);

  explicit WellKnownChangePasswordState(
      password_manager::WellKnownChangePasswordStateDelegate* delegate);
  ~WellKnownChangePasswordState();
  // Request the status code from a path that is expected to return 404.
  // In order to avoid security issues `request_initiator` and `trusted_params`
  // need to be derived from the initial navigation. These are not set on iOS.
  void FetchNonExistingResource(
      network::SharedURLLoaderFactory* url_loader_factory,
      const GURL& origin,
      std::optional<url::Origin> request_initiator = std::nullopt,
      std::optional<network::ResourceRequest::TrustedParams> trusted_params =
          std::nullopt);
  // Prefetch change password URLs from |affiliation_service|.
  void PrefetchChangePasswordURLs(
      affiliations::AffiliationService* affiliation_service,
      const std::vector<GURL>& urls);
  // The request to .well-known/change-password is not made by this State. To
  // get the response code for the request the owner of the state has to call
  // this method to tell the state.
  void SetChangePasswordResponseCode(int status_code);

 private:
  // Callback for the request to the "not exist" path.
  void FetchNonExistingResourceCallback(
      scoped_refptr<net::HttpResponseHeaders> headers);
  // Callback for the request to the Affiliation Service prefetch.
  void PrefetchChangePasswordURLsCallback();
  // Function is called when both requests are finished. Decides to continue or
  // redirect to change password URL or homepage.
  void ContinueProcessing();
  // Checks if both requests are finished.
  bool BothRequestsFinished() const;
  // Checks the status codes and returns if .well-known/change-password is
  // supported.
  bool SupportsWellKnownChangePasswordUrl() const;

  raw_ptr<WellKnownChangePasswordStateDelegate> delegate_ = nullptr;
  int non_existing_resource_response_code_ = 0;
  int change_password_response_code_ = 0;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::OneShotTimer prefetch_timer_;
  base::WeakPtrFactory<WellKnownChangePasswordState> weak_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_WELL_KNOWN_CHANGE_PASSWORD_WELL_KNOWN_CHANGE_PASSWORD_STATE_H_
