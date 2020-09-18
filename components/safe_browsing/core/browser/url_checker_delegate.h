// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_URL_CHECKER_DELEGATE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_URL_CHECKER_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"

namespace content {
class WebContents;
}

namespace net {
class HttpRequestHeaders;
}

namespace security_interstitials {
struct UnsafeResource;
}

namespace safe_browsing {

class BaseUIManager;
class SafeBrowsingDatabaseManager;

// Delegate interface for SafeBrowsingUrlCheckerImpl and SafeBrowsing's
// content::ResourceThrottle subclasses. They delegate to this interface those
// operations that different embedders (Chrome and Android WebView) handle
// differently.
//
// All methods should only be called from the IO thread.
class UrlCheckerDelegate
    : public base::RefCountedThreadSafe<UrlCheckerDelegate> {
 public:
  // Destroys prerender contents if necessary. The parameter is a
  // WebContents::OnceGetter, but that type is not visible from here.
  virtual void MaybeDestroyPrerenderContents(
      base::OnceCallback<content::WebContents*()> web_contents_getter) = 0;

  // Starts displaying the SafeBrowsing interstitial page.
  virtual void StartDisplayingBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      bool is_main_frame,
      bool has_user_gesture) = 0;

  // Starts observing user input events to display a SafeBrowsing interstitial
  // page when an event is received.
  virtual void StartObservingInteractionsForDelayedBlockingPageHelper(
      const security_interstitials::UnsafeResource& resource,
      bool is_main_frame) = 0;

  // An allowlisted URL is considered safe and therefore won't be checked with
  // the SafeBrowsing database.
  virtual bool IsUrlAllowlisted(const GURL& url) = 0;

  // If the method returns true, the entire request won't be checked, including
  // the original URL and redirects.
  // If neither of |render_process_id| and |render_frame_id| is -1, they will be
  // used to identify the frame making the request; otherwise
  // |frame_tree_node_id| will be used. Please note that |frame_tree_node_id|
  // could also be -1, if a request is not associated with a frame.
  virtual bool ShouldSkipRequestCheck(
      const GURL& original_url,
      int frame_tree_node_id,
      int render_process_id,
      int render_frame_id,
      bool originated_from_service_worker) = 0;

  // Notifies the SafeBrowsing Trigger Manager that a suspicious site has been
  // detected. |web_contents_getter| is used to determine which tab the site
  // was detected on.
  virtual void NotifySuspiciousSiteDetected(
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter) = 0;

  virtual const SBThreatTypeSet& GetThreatTypes() = 0;
  virtual SafeBrowsingDatabaseManager* GetDatabaseManager() = 0;
  virtual BaseUIManager* GetUIManager() = 0;

 protected:
  friend class base::RefCountedThreadSafe<UrlCheckerDelegate>;
  virtual ~UrlCheckerDelegate() {}
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_URL_CHECKER_DELEGATE_H_
