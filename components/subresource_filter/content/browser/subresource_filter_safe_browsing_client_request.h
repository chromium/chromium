// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_CLIENT_REQUEST_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_CLIENT_REQUEST_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/util.h"

class GURL;

namespace safe_browsing {
struct ThreatMetadata;
}  // namespace safe_browsing

namespace subresource_filter {

class SubresourceFilterSafeBrowsingClient;

// This class is scoped to a single database check, and it lives on the UI
// thread exclusively.
class SubresourceFilterSafeBrowsingClientRequest
    : public safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  SubresourceFilterSafeBrowsingClientRequest(
      size_t request_id,
      base::TimeTicks start_time_,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      SubresourceFilterSafeBrowsingClient* client);

  SubresourceFilterSafeBrowsingClientRequest(
      const SubresourceFilterSafeBrowsingClientRequest&) = delete;
  SubresourceFilterSafeBrowsingClientRequest& operator=(
      const SubresourceFilterSafeBrowsingClientRequest&) = delete;

  ~SubresourceFilterSafeBrowsingClientRequest() override;

  void Start(const GURL& url);

  // safe_browsing::SafeBrowsingDatabaseManager::Client:
  void OnCheckBrowseUrlResult(
      const GURL& url,
      safe_browsing::SBThreatType threat_type,
      const safe_browsing::ThreatMetadata& metadata) override;

  size_t request_id() const { return request_id_; }

  // Maximum time in milliseconds to wait for the Safe Browsing service to
  // verify a URL. After this amount of time the outstanding check will be
  // aborted, and the URL will be treated as if it didn't belong to the
  // Subresource Filter only list.
  static constexpr base::TimeDelta kCheckURLTimeout = base::Seconds(5);

 private:
  // Callback for when the safe browsing check has taken longer than
  // kCheckURLTimeout.
  void OnCheckUrlTimeout();

  void SendCheckResultToClient(bool served_from_network,
                               safe_browsing::SBThreatType threat_type,
                               const safe_browsing::ThreatMetadata& metadata);

  // The |request_id_| identifies a particular request, as issued from the
  // SubresourceFilterSafeBrowsingClient. It will be unique in the scope of a
  // single navigation (i.e. the scope of the
  // SubresourceFilterSafeBrowsingClient).
  const size_t request_id_;

  // The time when the request started, measured on the UI thread.
  const base::TimeTicks start_time_;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  raw_ptr<SubresourceFilterSafeBrowsingClient> client_ = nullptr;

  // Timer to abort the safe browsing check if it takes too long.
  base::OneShotTimer timer_;

  bool request_completed_ = false;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_CLIENT_REQUEST_H_
