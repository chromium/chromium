// Copyright 2020 The Chromium Authors
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
#include "base/version.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace url {
class Origin;
}

namespace password_manager {

extern const char kDefaultChangePasswordScriptsListUrl[];

class PasswordScriptsFetcherImpl
    : public password_manager::PasswordScriptsFetcher {
 public:
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
      bool is_supervised_user,
      const base::Version& version,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  PasswordScriptsFetcherImpl(
      bool is_supervised_user,
      const base::Version& version,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string scripts_list_url);

  ~PasswordScriptsFetcherImpl() override;

  // PasswordScriptsFetcher:
  void PrewarmCache() override;
  void RefreshScriptsIfNecessary(
      base::OnceClosure fetch_finished_callback) override;
  void FetchScriptAvailability(const url::Origin& origin,
                               ResponseCallback callback) override;
  bool IsScriptAvailable(const url::Origin& origin) const override;
  bool IsCacheStale() const override;
  base::Value::Dict GetDebugInformationForInternals() const override;
  base::Value::List GetCacheEntries() const override;

#if defined(UNIT_TEST)
  void make_cache_stale_for_testing() {
    last_fetch_timestamp_ = base::TimeTicks::Now() - base::Days(1);
  }
#endif

 private:
  // Sends new request to gstatic.
  void StartFetch();

  // Callback for the request to gstatic.
  void OnFetchComplete(base::TimeTicks request_start_timestamp,
                       std::unique_ptr<std::string> response_body);

  // Parses |response_body| and stores the result in `password_change_domains_`
  // (always overwrites the old list). Sets an empty list if `response_body` is
  // invalid. Returns a parsing result for a histogram. The function tries to be
  // forgiving and rather return warnings and skip an entry than cancel the
  // parsing.
  base::flat_set<ParsingResult> ParseResponse(
      std::unique_ptr<std::string> response_body);

  // Runs `callback` immediately with the script availability for `origin`.
  void RunResponseCallback(url::Origin origin, ResponseCallback callback);

  // Indicates whether the user has a supervised account - for those, script
  // availability already returns `false` unless overwritten by the
  // `kForceEnablePasswordDomainCapabilities` feature.
  const bool is_supervised_user_;

  const base::Version version_;

  // URL to fetch a list of scripts from.
  const std::string scripts_list_url_;

  // Parsed set of domains from gstatic.
  base::flat_map<url::Origin, base::Version> password_change_domains_;
  // Timestamp of the last finished request.
  base::TimeTicks last_fetch_timestamp_;
  // Stores the callbacks that are waiting for the request to finish.
  std::vector<base::OnceClosure> fetch_finished_callbacks_;
  // Stores the per-origin callbacks that are waiting for the request to finish.
  std::vector<std::pair<url::Origin, ResponseCallback>> pending_callbacks_;
  // URL loader object for the gstatic request. If |url_loader_| is not null, a
  // request is currently in flight.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // Used for the gstatic requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_IMPL_H_
