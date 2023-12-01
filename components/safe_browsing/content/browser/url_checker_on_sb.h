// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_URL_CHECKER_ON_SB_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_URL_CHECKER_ON_SB_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace net {
class HttpRequestHeaders;
}

namespace safe_browsing {

class UrlCheckerDelegate;
class SafeBrowsingLookupMechanismExperimenter;

class RealTimeUrlLookupServiceBase;
class HashRealTimeService;
class PingManager;

// UrlCheckerOnSB handles calling methods on SafeBrowsingUrlCheckerImpl, which
// must be called on the IO thread. The results are synced back to the
// throttle.
// TODO(http://crbug.com/824843): Remove this if safe browsing is moved to the
// UI thread.
class UrlCheckerOnSB : public base::SupportsWeakPtr<UrlCheckerOnSB> {
 public:
  using OnCompleteCheckCallback = base::RepeatingCallback<void(
      bool /* slow_check */,
      bool /* proceed */,
      bool /* showed_interstitial */,
      SafeBrowsingUrlCheckerImpl::PerformedCheck /* performed_check */)>;

  using OnNotifySlowCheckCallback = base::RepeatingCallback<void()>;

  using GetDelegateCallback =
      base::RepeatingCallback<scoped_refptr<UrlCheckerDelegate>()>;

  using NativeUrlCheckNotifier = base::OnceCallback<void(
      bool /* proceed */,
      bool /* showed_interstitial */,
      SafeBrowsingUrlCheckerImpl::PerformedCheck /* performed_check */)>;

  UrlCheckerOnSB(
      GetDelegateCallback delegate_getter,
      int frame_tree_node_id,
      base::RepeatingCallback<content::WebContents*()> web_contents_getter,
      OnCompleteCheckCallback complete_callback,
      OnNotifySlowCheckCallback slow_check_callback,
      bool url_real_time_lookup_enabled,
      bool can_urt_check_subresource_url,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
      base::WeakPtr<PingManager> ping_manager,
      bool is_mechanism_experiment_allowed,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection);

  ~UrlCheckerOnSB();

  // Starts the initial safe browsing check.
  void Start(const net::HttpRequestHeaders& headers,
             int load_flags,
             network::mojom::RequestDestination request_destination,
             bool has_user_gesture,
             const GURL& url,
             const std::string& method);

  // Checks the specified |url| using |url_checker_|.
  void CheckUrl(const GURL& url, const std::string& method);

  void LogWillProcessResponseTime(base::TimeTicks reached_time);

  void SetUrlCheckerForTesting(
      std::unique_ptr<SafeBrowsingUrlCheckerImpl> checker);

 private:
  // If |slow_check_notifier| is non-null, it indicates that a "slow check" is
  // ongoing, i.e., the URL may be unsafe and a more time-consuming process is
  // required to get the final result. In that case, the rest of the callback
  // arguments should be ignored. This method sets the |slow_check_notifier|
  // output parameter to a callback to receive the final result.
  void OnCheckUrlResult(
      NativeUrlCheckNotifier* slow_check_notifier,
      bool proceed,
      bool showed_interstitial,
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check);

  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments of OnCheckUrlResult() for what slow check means).
  void OnCompleteCheck(
      bool slow_check,
      bool proceed,
      bool showed_interstitial,
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check);

  // The following member stays valid until |url_checker_| is created.
  GetDelegateCallback delegate_getter_;

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;
  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_for_testing_;
  int frame_tree_node_id_;
  scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
      mechanism_experimenter_;
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  OnCompleteCheckCallback complete_callback_;
  OnNotifySlowCheckCallback slow_check_callback_;
  bool url_real_time_lookup_enabled_ = false;
  bool can_urt_check_subresource_url_ = false;
  bool can_check_db_ = true;
  bool can_check_high_confidence_allowlist_ = true;
  std::string url_lookup_service_metric_suffix_;
  GURL last_committed_url_;
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_;
  base::WeakPtr<HashRealTimeService> hash_realtime_service_;
  base::WeakPtr<PingManager> ping_manager_;
  bool is_mechanism_experiment_allowed_ = false;
  hash_realtime_utils::HashRealTimeSelection hash_realtime_selection_ =
      hash_realtime_utils::HashRealTimeSelection::kNone;
  base::TimeTicks creation_time_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_URL_CHECKER_ON_SB_H_
