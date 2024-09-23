// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_URL_CHECKER_HOLDER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_URL_CHECKER_HOLDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace net {
class HttpRequestHeaders;
}

namespace safe_browsing {

class UrlCheckerDelegate;

class RealTimeUrlLookupServiceBase;
class HashRealTimeService;

// UrlCheckerHolder handles calling methods on SafeBrowsingUrlCheckerImpl.
class UrlCheckerHolder final {
 public:
  struct StartParams {
    StartParams(net::HttpRequestHeaders headers,
                int load_flags,
                bool has_user_gesture,
                GURL url,
                std::string method);
    StartParams(const StartParams& other);
    ~StartParams();
    net::HttpRequestHeaders headers;
    int load_flags;
    bool has_user_gesture;
    const GURL url;
    const std::string method;
  };

  struct OnCompleteCheckResult {
    OnCompleteCheckResult(
        bool proceed,
        bool showed_interstitial,
        bool has_post_commit_interstitial_skipped,
        SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
        bool all_checks_completed);
    bool proceed;
    bool showed_interstitial;
    bool has_post_commit_interstitial_skipped;
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check;
    bool all_checks_completed;
  };

  using OnCompleteCheckCallback =
      base::RepeatingCallback<void(OnCompleteCheckResult)>;

  using GetDelegateCallback =
      base::RepeatingCallback<scoped_refptr<UrlCheckerDelegate>()>;

  UrlCheckerHolder(
      GetDelegateCallback delegate_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id,
      base::RepeatingCallback<content::WebContents*()> web_contents_getter,
      OnCompleteCheckCallback complete_callback,
      bool url_real_time_lookup_enabled,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
      bool is_async_check,
      bool check_allowlist_before_hash_database,
      SessionID tab_id);

  ~UrlCheckerHolder();

  // Starts the initial safe browsing check.
  void Start(const StartParams& params);

  // Checks the specified |url| using |url_checker_|.
  void CheckUrl(const GURL& url, const std::string& method);

  // Replaces the current |complete_callback_| with the new |callback|.
  void SwapCompleteCallback(OnCompleteCheckCallback callback);

  // Returns a list of URLs that are checked by |url_checker_|.
  const std::vector<GURL>& GetRedirectChain();

  void SetUrlCheckerForTesting(
      std::unique_ptr<SafeBrowsingUrlCheckerImpl> checker);

  std::optional<int64_t> navigation_id() { return navigation_id_; }

  bool IsRealTimeCheckForTesting();

  bool IsAsyncCheckForTesting();

  bool IsCheckAllowlistBeforeHashDatabaseForTesting();

  void AddUrlInRedirectChainForTesting(const GURL& url);

  base::WeakPtr<UrlCheckerHolder> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // This is the callback invoked by |url_checker_|. See the UrlCheckResult
  // struct in |UnsafeResource| for the meaning of each parameter.
  void OnCheckUrlResult(
      bool proceed,
      bool showed_interstitial,
      bool has_post_commit_interstitial_skipped,
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check);

  // The following member stays valid until |url_checker_| is created.
  GetDelegateCallback delegate_getter_;

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;
  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_for_testing_;
  content::FrameTreeNodeId frame_tree_node_id_;
  std::optional<int64_t> navigation_id_;
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  OnCompleteCheckCallback complete_callback_;
  bool url_real_time_lookup_enabled_ = false;
  bool can_check_db_ = true;
  bool can_check_high_confidence_allowlist_ = true;
  size_t pending_checks_ = 0;
  std::string url_lookup_service_metric_suffix_;
  // A list of URLs that are checked by |url_checker_|.
  std::vector<GURL> redirect_chain_;
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_;
  base::WeakPtr<HashRealTimeService> hash_realtime_service_;
  hash_realtime_utils::HashRealTimeSelection hash_realtime_selection_ =
      hash_realtime_utils::HashRealTimeSelection::kNone;
  base::TimeTicks creation_time_;
  bool is_async_check_ = false;
  bool check_allowlist_before_hash_database_ = false;
  SessionID tab_id_;
  base::WeakPtrFactory<UrlCheckerHolder> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_URL_CHECKER_HOLDER_H_
