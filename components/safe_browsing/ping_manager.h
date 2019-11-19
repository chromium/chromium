// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PING_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_PING_MANAGER_H_

// A class that reports basic safebrowsing statistics to Google's SafeBrowsing
// servers.
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/db/hit_report.h"
#include "components/safe_browsing/db/util.h"
#include "content/public/browser/permission_type.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class PingManager {
 public:
  virtual ~PingManager();

  // Create an instance of the safe browsing ping manager.
  static std::unique_ptr<PingManager> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& config);

  void OnURLLoaderComplete(network::SimpleURLLoader* source,
                           std::unique_ptr<std::string> response_body);

  // Report to Google when a SafeBrowsing warning is shown to the user.
  // |hit_report.threat_type| should be one of the types known by
  // SafeBrowsingtHitUrl.
  void ReportSafeBrowsingHit(const safe_browsing::HitReport& hit_report);

  // Users can opt-in on the SafeBrowsing interstitial to send detailed
  // threat reports. |report| is the serialized report.
  void ReportThreatDetails(const std::string& report);

 protected:
  friend class PingManagerTest;
  // Constructs a PingManager that issues network requests
  // using |url_loader_factory|.
  PingManager(scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const V4ProtocolConfig& config);

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

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Track outstanding SafeBrowsing report fetchers for clean up.
  // We add both "hit" and "detail" fetchers in this set.
  Reports safebrowsing_reports_;

  DISALLOW_COPY_AND_ASSIGN(PingManager);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PING_MANAGER_H_
