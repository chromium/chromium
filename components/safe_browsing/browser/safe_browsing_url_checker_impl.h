// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/proto/realtimeapi.pb.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class UrlCheckerDelegate;

// A SafeBrowsingUrlCheckerImpl instance is used to perform SafeBrowsing check
// for a URL and its redirect URLs. It implements Mojo interface so that it can
// be used to handle queries from renderers. But it is also used to handle
// queries from the browser. In that case, the public methods are called
// directly instead of through Mojo.
//
// To be considered "safe", a URL must not appear in the SafeBrowsing blacklists
// (see SafeBrowsingService for details).
//
// Note that the SafeBrowsing check takes at most kCheckUrlTimeoutMs
// milliseconds. If it takes longer than this, then the system defaults to
// treating the URL as safe.
//
// If the URL is classified as dangerous, a warning interstitial page is
// displayed. In that case, the user can click through the warning page if they
// decides to procced with loading the URL anyway.
class SafeBrowsingUrlCheckerImpl : public mojom::SafeBrowsingUrlChecker,
                                   public SafeBrowsingDatabaseManager::Client {
 public:
  using NativeUrlCheckNotifier =
      base::OnceCallback<void(bool /* proceed */,
                              bool /* showed_interstitial */)>;

  // If |slow_check_notifier| is not null, the callback is supposed to update
  // this output parameter with a callback to receive complete notification. In
  // that case, |proceed| and |showed_interstitial| should be ignored.
  using NativeCheckUrlCallback =
      base::OnceCallback<void(NativeUrlCheckNotifier* /* slow_check_notifier */,
                              bool /* proceed */,
                              bool /* showed_interstitial */)>;

  // Constructor for SafeBrowsingUrlCheckerImpl. |real_time_lookup_enabled|
  // indicates whether or not the profile has enabled real time URL lookups, as
  // computed by the RealTimePolicyEngine. This must be computed in advance,
  // since this class only exists on the IO thread.
  SafeBrowsingUrlCheckerImpl(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      content::ResourceType resource_type,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::Callback<content::WebContents*()>& web_contents_getter,
      bool real_time_lookup_enabled);

  ~SafeBrowsingUrlCheckerImpl() override;

  // mojom::SafeBrowsingUrlChecker implementation.
  // NOTE: |callback| could be run synchronously before this method returns. Be
  // careful if |callback| could destroy this object.
  void CheckUrl(const GURL& url,
                const std::string& method,
                CheckUrlCallback callback) override;

  // NOTE: |callback| could be run synchronously before this method returns. Be
  // careful if |callback| could destroy this object.
  void CheckUrl(const GURL& url,
                const std::string& method,
                NativeCheckUrlCallback callback);

 private:
  class Notifier {
   public:
    explicit Notifier(CheckUrlCallback callback);
    explicit Notifier(NativeCheckUrlCallback native_callback);

    ~Notifier();

    Notifier(Notifier&& other);
    Notifier& operator=(Notifier&& other);

    void OnStartSlowCheck();
    void OnCompleteCheck(bool proceed, bool showed_interstitial);

   private:
    // Used in the mojo interface case.
    CheckUrlCallback callback_;
    mojo::Remote<mojom::UrlCheckNotifier> slow_check_notifier_;

    // Used in the native call case.
    NativeCheckUrlCallback native_callback_;
    NativeUrlCheckNotifier native_slow_check_notifier_;
  };

  // SafeBrowsingDatabaseManager::Client implementation:
  void OnCheckBrowseUrlResult(const GURL& url,
                              SBThreatType threat_type,
                              const ThreatMetadata& metadata) override;

  void OnCheckUrlForHighConfidenceAllowlist(bool did_match_allowlist) override;

  void OnTimeout();

  void OnUrlResult(const GURL& url,
                   SBThreatType threat_type,
                   const ThreatMetadata& metadata);

  void CheckUrlImpl(const GURL& url,
                    const std::string& method,
                    Notifier notifier);

  // NOTE: this method runs callbacks which could destroy this object.
  void ProcessUrls();

  // NOTE: this method runs callbacks which could destroy this object.
  void BlockAndProcessUrls(bool showed_interstitial);

  void OnBlockingPageComplete(bool proceed);

  // Helper method that checks whether |url|'s reputation can be checked using
  // real time lookups.
  bool CanPerformFullURLLookup(const GURL& url);

  SBThreatType CheckWebUIUrls(const GURL& url);

  // Returns false if this object has been destroyed by the callback. In that
  // case none of the members of this object should be touched again.
  bool RunNextCallback(bool proceed, bool showed_interstitial);

  // Called when the |request| from the real-time lookup service is sent.
  void OnRTLookupRequest(std::unique_ptr<RTLookupRequest> request);

  // Called when the |response| from the real-time lookup service is received.
  void OnRTLookupResponse(std::unique_ptr<RTLookupResponse> response);

  void SetWebUIToken(int token);

  enum State {
    // Haven't started checking or checking is complete.
    STATE_NONE,
    // We have one outstanding URL-check.
    STATE_CHECKING_URL,
    // We're displaying a blocking page.
    STATE_DISPLAYING_BLOCKING_PAGE,
    // The blocking page has returned *not* to proceed.
    STATE_BLOCKED
  };

  struct UrlInfo {
    UrlInfo(const GURL& url, const std::string& method, Notifier notifier);
    UrlInfo(UrlInfo&& other);

    ~UrlInfo();

    GURL url;
    std::string method;
    Notifier notifier;
  };

  const net::HttpRequestHeaders headers_;
  const int load_flags_;
  const content::ResourceType resource_type_;
  const bool has_user_gesture_;
  base::Callback<content::WebContents*()> web_contents_getter_;
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The redirect chain for this resource, including the original URL and
  // subsequent redirect URLs.
  std::vector<UrlInfo> urls_;
  // |urls_| before |next_index_| have been checked. If |next_index_| is smaller
  // than the size of |urls_|, the URL at |next_index_| is being processed.
  size_t next_index_ = 0;

  // Token used for displaying url real time lookup pings. A single token is
  // sufficient since real time check only happens on main frame url.
  int url_web_ui_token_ = -1;

  State state_ = STATE_NONE;

  // Timer to abort the SafeBrowsing check if it takes too long.
  base::OneShotTimer timer_;

  // Whether real time lookup is enabled for this request.
  bool real_time_lookup_enabled_;

  base::WeakPtrFactory<SafeBrowsingUrlCheckerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUrlCheckerImpl);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
