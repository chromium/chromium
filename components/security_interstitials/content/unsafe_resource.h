// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/safe_browsing/db/hit_report.h"
#include "components/safe_browsing/db/util.h"
#include "url/gurl.h"

namespace content {
class NavigationEntry;
class WebContents;
}  // namespace content

namespace security_interstitials {

// Structure that passes parameters between the IO and UI thread when
// interacting with the safe browsing blocking page.
struct UnsafeResource {
  // Passed a boolean indicating whether or not it is OK to proceed with
  // loading an URL.
  typedef base::Callback<void(bool /*proceed*/)> UrlCheckCallback;

  UnsafeResource();
  UnsafeResource(const UnsafeResource& other);
  ~UnsafeResource();

  // Returns true if this UnsafeResource is a main frame load that was blocked
  // while the navigation is still pending. Note that a main frame hit may not
  // be blocking, eg. client side detection happens after the load is
  // committed.
  bool IsMainPageLoadBlocked() const;

  // Returns the NavigationEntry for this resource (for a main frame hit) or
  // for the page which contains this resource (for a subresource hit).
  // This method must only be called while the UnsafeResource is still
  // "valid".
  // I.e,
  //   For MainPageLoadBlocked resources, it must not be called if the load
  //   was aborted (going back or replaced with a different navigation),
  //   or resumed (proceeded through warning or matched whitelist).
  //   For non-MainPageLoadBlocked resources, it must not be called if any
  //   other navigation has committed (whether by going back or unrelated
  //   navigations), though a pending navigation is okay.
  content::NavigationEntry* GetNavigationEntryForResource() const;

  // Helper to build a getter for WebContents* from render frame id.
  static base::Callback<content::WebContents*(void)> GetWebContentsGetter(
      int render_process_host_id,
      int render_frame_id);

  GURL url;
  GURL original_url;
  GURL navigation_url;
  GURL referrer_url;
  std::vector<GURL> redirect_urls;
  bool is_subresource;
  bool is_subframe;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatMetadata threat_metadata;
  UrlCheckCallback callback;  // This is called back on |callback_thread|.
  scoped_refptr<base::SingleThreadTaskRunner> callback_thread;
  base::Callback<content::WebContents*(void)> web_contents_getter;
  safe_browsing::ThreatSource threat_source;
  // |token| field is only set if |threat_type| is
  // SB_THREAT_TYPE_*_PASSWORD_REUSE.
  std::string token;
};

}  // security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_H_
