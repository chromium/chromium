// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace url {
class Origin;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace base {
class Version;
}

namespace password_manager {

extern const char kDefaultChangePasswordScriptsListUrl[];

class PasswordScriptsFetcherImpl
    : public password_manager::PasswordScriptsFetcher {
 public:
  // These enums are used in histograms. Do not change or reuse values.
  enum class CacheState {
    // Cache is ready.
    kReady = 0,
    // Cache was set but it is stale. Re-fetch needed.
    kStale = 1,
    // Cache was never set,
    kNeverSet = 2,
    // Cache is waiting for an in-flight request.
    kWaiting = 3,
    kMaxValue = kWaiting,
  };
  enum class ParsingResult {
    // No response from the server.
    kNoResponse = 0,
    // There was at least one invalid URL.
    kInvalidUrl = 3,
    // No errors occurred.
    kOk = 4,
    // Invalid JSON (either syntactically, e.g. ill-formed lists, dictionaries,
    // strings, etc., or structurally, e.g. a dictionary that does not contain
    // the expected keys).
    kInvalidJson = 5,
    kMaxValue = kInvalidJson,
  };

  // The first constructor calls the second one. The second one is called
  // directly only from tests.
  PasswordScriptsFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  PasswordScriptsFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string scripts_list_url);

  ~PasswordScriptsFetcherImpl() override;

  // PasswordScriptsFetcher:
  void PrewarmCache() override;
  void RefreshScriptsIfNecessary(
      base::OnceClosure fetch_finished_callback) override;
  void FetchScriptAvailability(const url::Origin& origin,
                               const base::Version& version,
                               ResponseCallback callback) override;
  bool IsScriptAvailable(const url::Origin& origin,
                         const base::Version& version) const override;

#if defined(UNIT_TEST)
  void make_cache_stale_for_testing() {
    last_fetch_timestamp_ =
        base::TimeTicks::Now() - base::TimeDelta::FromDays(1);
  }
#endif

 private:
  // Sends new request to gstatic.
  void StartFetch();
  // Callback for the request to gstatic.
  void OnFetchComplete(base::TimeTicks request_start_timestamp,
                       std::unique_ptr<std::string> response_body);
  // Parses |response_body| and stores the result in |password_change_domains_|
  // (always overwrites the old list). Sets an empty list if |response_body| is
  // invalid. Returns a parsing result for a histogram. The function tries to be
  // forgiving and rather return warnings and skip an entry than cancel the
  // parsing.
  base::flat_set<ParsingResult> ParseResponse(
      std::unique_ptr<std::string> response_body);
  // Returns whether a re-fetch is needed.
  bool IsCacheStale() const;
  // Runs |callback| immediately with the script availability for |origin|.
  void RunResponseCallback(url::Origin origin,
                           base::Version version,
                           ResponseCallback callback);

  // URL to fetch a list of scripts from.
  const std::string scripts_list_url_;

  // Parsed set of domains from gstatic.
  base::flat_map<url::Origin, base::Version> password_change_domains_;
  // Timestamp of the last finished request.
  base::TimeTicks last_fetch_timestamp_;
  // Stores the callbacks that are waiting for the request to finish.
  std::vector<base::OnceClosure> fetch_finished_callbacks_;
  // Stores the per-origin callbacks that are waiting for the request to finish.
  std::vector<
      std::pair<std::pair<url::Origin, base::Version>, ResponseCallback>>
      pending_callbacks_;
  // URL loader object for the gstatic request. If |url_loader_| is not null, a
  // request is currently in flight.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // Used for the gstatic requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_IMPL_H_
