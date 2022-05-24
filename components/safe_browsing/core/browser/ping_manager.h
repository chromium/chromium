// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PING_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PING_MANAGER_H_

// A class that reports basic safebrowsing statistics to Google's SafeBrowsing
// servers.
#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class PingManager : public KeyedService {
 public:
  PingManager(const PingManager&) = delete;
  PingManager& operator=(const PingManager&) = delete;

  ~PingManager() override;

  // Create an instance of the safe browsing ping manager.
  static PingManager* Create(
      const V4ProtocolConfig& config,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      base::RepeatingCallback<bool()> get_should_fetch_access_token);

  void OnURLLoaderComplete(network::SimpleURLLoader* source,
                           std::unique_ptr<std::string> response_body);
  void OnThreatDetailsReportURLLoaderComplete(
      network::SimpleURLLoader* source,
      bool has_access_token,
      std::unique_ptr<std::string> response_body);

  // Report to Google when a SafeBrowsing warning is shown to the user.
  // |hit_report.threat_type| should be one of the types known by
  // SafeBrowsingtHitUrl.
  void ReportSafeBrowsingHit(const safe_browsing::HitReport& hit_report);

  // Users can opt-in on the SafeBrowsing interstitial to send detailed
  // threat reports. |report| is the serialized report.
  void ReportThreatDetails(const std::string& report);

  // Only used for tests
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  void SetTokenFetcherForTesting(
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher);

 protected:
  friend class PingManagerTest;
  // Constructs a PingManager with the given |config|, |url_loader_factory|, and
  // access token fetching information.
  explicit PingManager(
      const V4ProtocolConfig& config,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      base::RepeatingCallback<bool()> get_should_fetch_access_token);

 private:
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestSafeBrowsingHitUrl);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestThreatDetailsUrl);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestReportThreatDetails);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestReportSafeBrowsingHit);

  const V4ProtocolConfig config_;

  using Reports = std::set<std::unique_ptr<network::SimpleURLLoader>,
                           base::UniquePtrComparator>;

  // Generates URL for reporting safe browsing hits.
  GURL SafeBrowsingHitUrl(const safe_browsing::HitReport& hit_report) const;

  // Generates URL for reporting threat details for users who opt-in.
  GURL ThreatDetailsUrl() const;

  // Once the user's access_token has been fetched by ReportThreatDetails (or
  // intentionally not fetched), attaches the token and sends the report.
  void ReportThreatDetailsOnGotAccessToken(const std::string& report,
                                           const std::string& access_token);

  // Track outstanding SafeBrowsing report fetchers for clean up.
  // We add both "hit" and "detail" fetchers in this set.
  Reports safebrowsing_reports_;

  // Used to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // Determines whether it's relevant to fetch the access token for the user
  // based on whether they're a signed-in ESB user.
  base::RepeatingCallback<bool()> get_should_fetch_access_token_;

  base::WeakPtrFactory<PingManager> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PING_MANAGER_H_
