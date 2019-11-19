// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_CLIENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_CLIENT_H_

#include <stddef.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/safe_browsing/db/util.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}  // namespace safe_browsing

namespace subresource_filter {

class SubresourceFilterSafeBrowsingActivationThrottle;
class SubresourceFilterSafeBrowsingClientRequest;

// Created on the UI thread but used on the IO thread to communicate with the
// safe browsing service.
//
// The class is expected to accompany a single navigation, and can maintain many
// database requests. It will cancel any outgoing requests when it is destroyed.
class SubresourceFilterSafeBrowsingClient {
 public:
  struct CheckResult {
    size_t request_id = 0;
    safe_browsing::SBThreatType threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE;

    // The metadata should generally be lightweight enough to copy around
    // without performance implications. Refactor this class if that ever
    // changes.
    safe_browsing::ThreatMetadata threat_metadata;
    base::TimeTicks start_time;
    bool finished = false;

    std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;
  };

  SubresourceFilterSafeBrowsingClient(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      base::WeakPtr<SubresourceFilterSafeBrowsingActivationThrottle> throttle,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> throttle_task_runner);

  ~SubresourceFilterSafeBrowsingClient();

  void CheckUrlOnIO(const GURL& url,
                    size_t request_id,
                    base::TimeTicks start_time);

  void OnCheckBrowseUrlResult(
      SubresourceFilterSafeBrowsingClientRequest* request,
      const CheckResult& check_result);

 private:
  // This is stored as a map to allow for ergonomic deletion.
  base::flat_map<SubresourceFilterSafeBrowsingClientRequest*,
                 std::unique_ptr<SubresourceFilterSafeBrowsingClientRequest>>
      requests_;

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;

  base::WeakPtr<SubresourceFilterSafeBrowsingActivationThrottle> throttle_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> throttle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingClient);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_SAFE_BROWSING_CLIENT_H_
