// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_FETCHER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_FETCHER_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace autofill {

class PasswordRequirementsSpec;

// A concrete implementation for PasswordRequirementsSpecFetcher that talks
// to the network.
class PasswordRequirementsSpecFetcherImpl
    : public PasswordRequirementsSpecFetcher {
 public:
  // This enum is used in histograms. Do not change or reuse values.
  enum class ResultCode {
    // Fetched spec file, parsed it, but found no entry for the origin.
    kFoundNoSpec = 0,
    // Fetched spec file, parsed it and found an entry.
    kFoundSpec = 1,
    // The origin is an IP address, not HTTP/HTTPS, or not a valid URL.
    kErrorInvalidOrigin = 2,
    // Server responded with an empty document or an error code.
    kErrorFailedToFetch = 3,
    // Server timed out.
    kErrorTimeout = 4,
    // Server responded with a document but it could not be parsed.
    kErrorFailedToParse = 5,
    // No URL loader configured for the PasswordRequirementsSpecFetcher.
    kErrorNoUrlLoader = 6,
    kMaxValue = kErrorNoUrlLoader,
  };

  // See the member variables for explanations of these parameters.
  PasswordRequirementsSpecFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      int version,
      size_t prefix_length,
      int timeout);
  ~PasswordRequirementsSpecFetcherImpl() override;

  // Implementation for PasswordRequirementsSpecFetcher:
  void Fetch(GURL origin, FetchCallback callback) override;

 private:
  // This structure bundles all data that are associated to a network request
  // for a file with a specific hash prefix.
  struct LookupInFlight {
    LookupInFlight();
    ~LookupInFlight();

    // Callbacks to be called if the network request resolves or is aborted.
    // The GURL represents the origin due to which a spec was fetched.
    // Used a std::list instead of std::vector to grow this cheaply.
    std::list<std::pair<GURL, FetchCallback>> callbacks;

    // Timer to kill pending downloads after |timeout_|.
    base::OneShotTimer download_timer;

    std::unique_ptr<network::SimpleURLLoader> url_loader;

    // Time when the network request is started.
    base::TimeTicks start_of_request;

   private:
    DISALLOW_COPY_AND_ASSIGN(LookupInFlight);
  };

  // These are the two ways how a network request can end. The functions remove
  // the entry corresponding to |hash_prefix| out of |lookups_in_flight_| as
  // their first order of business.
  void OnFetchComplete(const std::string& hash_prefix,
                       std::unique_ptr<std::string> response_body);
  void OnFetchTimeout(const std::string& hash_prefix);

  // Calls all |callbacks| in order. Note that these callbacks are OnceCallback
  // instances. Therefore, entries are reset after this function returns.
  void TriggerCallbackToAll(
      std::list<std::pair<GURL, FetchCallback>>* callbacks,
      ResultCode result,
      const PasswordRequirementsSpec& spec);

  // Calls the |callback| with the specific data and records some metrics.
  void TriggerCallback(FetchCallback callback,
                       ResultCode result,
                       const PasswordRequirementsSpec& spec);

  std::unique_ptr<LookupInFlight> RemoveLookupInFlight(
      const std::string& hash_prefix);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // A version counter for requirements specs. If data changes on the server,
  // a new version number is pushed out to prevent that clients continue
  // using old cached data. This allows setting the HTTP caching expiration to
  // infinity.
  int version_;

  // The fetcher determines the URL of the spec file by first hashing the eTLD+1
  // of |origin| and then taking the first |prefix_length_| bits of the hash
  // value as part of the file name. (See the code for details.)
  // |prefix_length_| should always be <= 32 as filenames are limited to the
  // first 4 bytes of the hash prefix.
  size_t prefix_length_;

  // Timeout in milliseconds after which any ongoing fetch operation is
  // canceled.
  int timeout_;

  // Data about network requests in flight.
  // The key is the name of the file being fetched without the common URL prefix
  // (e.g. "0000"). The value contains callbacks that should process the result
  // and a timer to cancel the lookup after some time.
  // The invariant of |lookups_in_flight_| is that entries exist from the
  // time of starting the network request until receiving the response or a
  // timeout.
  std::map<std::string, std::unique_ptr<LookupInFlight>> lookups_in_flight_;

  DISALLOW_COPY_AND_ASSIGN(PasswordRequirementsSpecFetcherImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_FETCHER_IMPL_H_
