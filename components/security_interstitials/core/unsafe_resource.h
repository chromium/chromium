// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/db/hit_report.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace web {
class WebState;
}  // namespace web

namespace security_interstitials {

// Structure that passes parameters between the IO and UI thread when
// interacting with the safe browsing blocking page.
struct UnsafeResource {
  // Passed booleans indicating whether or not it is OK to proceed with
  // loading an URL and whether or not an interstitial was shown as a result of
  // the URL load, |showed_interstitial| should only be set to true if the
  // interstitial was shown as a direct result of the navigation to the URL.
  // (e.g. it should be set to true if the interstitial will be shown from a
  // navigation throttle triggered by this navigation, but to false if it will
  // be shown using LoadPostCommitErrorPage).
  using UrlCheckCallback =
      base::RepeatingCallback<void(bool /*proceed*/,
                                   bool /*showed_interstitial*/)>;

  UnsafeResource();
  UnsafeResource(const UnsafeResource& other);
  ~UnsafeResource();

  // Returns true if this UnsafeResource is a main frame load that was blocked
  // while the navigation is still pending. Note that a main frame hit may not
  // be blocking, eg. client side detection happens after the load is
  // committed.
  bool IsMainPageLoadBlocked() const;

  // Checks if |callback| is not null and posts it to |callback_thread|.
  void DispatchCallback(const base::Location& from_here,
                        bool proceed,
                        bool showed_interstitial) const;

  GURL url;
  GURL original_url;
  GURL navigation_url;
  GURL referrer_url;
  std::vector<GURL> redirect_urls;
  bool is_subresource;
  bool is_subframe;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatMetadata threat_metadata;
  network::mojom::RequestDestination request_destination;
  UrlCheckCallback callback;  // This is called back on |callback_thread|.
  scoped_refptr<base::SingleThreadTaskRunner> callback_thread;
  // TODO(crbug.com/1073315): |web_state_getter| is only used on iOS, and
  // |web_contents_getter| is used on all other platforms.  This struct should
  // be refactored to use only the common functionality can be shared across
  // platforms.
  base::RepeatingCallback<content::WebContents*(void)> web_contents_getter;
  base::RepeatingCallback<web::WebState*(void)> web_state_getter;
  safe_browsing::ThreatSource threat_source;
  // |token| field is only set if |threat_type| is
  // SB_THREAT_TYPE_*_PASSWORD_REUSE.
  std::string token;

  // If true, this UnsafeResource is created because of the Delayed Warnings
  // experiment.
  bool is_delayed_warning;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_
