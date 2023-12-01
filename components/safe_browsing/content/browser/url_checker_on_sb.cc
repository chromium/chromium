// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/url_checker_on_sb.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_experimenter.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

UrlCheckerOnSB::UrlCheckerOnSB(
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
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection)
    : delegate_getter_(std::move(delegate_getter)),
      frame_tree_node_id_(frame_tree_node_id),
      web_contents_getter_(web_contents_getter),
      complete_callback_(std::move(complete_callback)),
      slow_check_callback_(std::move(slow_check_callback)),
      url_real_time_lookup_enabled_(url_real_time_lookup_enabled),
      can_urt_check_subresource_url_(can_urt_check_subresource_url),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      url_lookup_service_(url_lookup_service),
      hash_realtime_service_(hash_realtime_service),
      ping_manager_(ping_manager),
      is_mechanism_experiment_allowed_(is_mechanism_experiment_allowed),
      hash_realtime_selection_(hash_realtime_selection),
      creation_time_(base::TimeTicks::Now()) {
  content::WebContents* contents = web_contents_getter_.Run();
  if (!!contents) {
    last_committed_url_ = contents->GetLastCommittedURL();
  }
}

UrlCheckerOnSB::~UrlCheckerOnSB() {
  base::UmaHistogramMediumTimes(
      "SafeBrowsing.BrowserThrottle.CheckerOnIOLifetime",
      base::TimeTicks::Now() - creation_time_);
  if (mechanism_experimenter_) {
    mechanism_experimenter_->OnBrowserUrlLoaderThrottleCheckerOnSBDestructed();
  }
}

void UrlCheckerOnSB::Start(
    const net::HttpRequestHeaders& headers,
    int load_flags,
    network::mojom::RequestDestination request_destination,
    bool has_user_gesture,
    const GURL& url,
    const std::string& method) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
      std::move(delegate_getter_).Run();

  if (is_mechanism_experiment_allowed_ &&
      request_destination == network::mojom::RequestDestination::kDocument) {
    mechanism_experimenter_ =
        base::MakeRefCounted<SafeBrowsingLookupMechanismExperimenter>(
            /*is_prefetch=*/load_flags & net::LOAD_PREFETCH,
            /*ping_manager_on_ui=*/ping_manager_,
            /*ui_task_runner=*/content::GetUIThreadTaskRunner({}));
  }
  if (url_checker_for_testing_) {
    url_checker_ = std::move(url_checker_for_testing_);
  } else {
    url_checker_ = std::make_unique<SafeBrowsingUrlCheckerImpl>(
        headers, load_flags, request_destination, has_user_gesture,
        url_checker_delegate, web_contents_getter_, nullptr,
        content::ChildProcessHost::kInvalidUniqueID, std::nullopt,
        frame_tree_node_id_, url_real_time_lookup_enabled_,
        can_urt_check_subresource_url_, can_check_db_,
        can_check_high_confidence_allowlist_, url_lookup_service_metric_suffix_,
        last_committed_url_, content::GetUIThreadTaskRunner({}),
        url_lookup_service_, WebUIInfoSingleton::GetInstance(),
        hash_realtime_service_, mechanism_experimenter_,
        is_mechanism_experiment_allowed_, hash_realtime_selection_);
  }

  CheckUrl(url, method);
}

void UrlCheckerOnSB::CheckUrl(const GURL& url, const std::string& method) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  DCHECK(url_checker_);
  url_checker_->CheckUrl(url, method,
                         base::BindOnce(&UrlCheckerOnSB::OnCheckUrlResult,
                                        base::Unretained(this)));
}

void UrlCheckerOnSB::LogWillProcessResponseTime(base::TimeTicks reached_time) {
  if (mechanism_experimenter_) {
    mechanism_experimenter_->OnWillProcessResponseReached(reached_time);
  }
}

void UrlCheckerOnSB::SetUrlCheckerForTesting(
    std::unique_ptr<SafeBrowsingUrlCheckerImpl> checker) {
  url_checker_for_testing_ = std::move(checker);
}

void UrlCheckerOnSB::OnCheckUrlResult(
    NativeUrlCheckNotifier* slow_check_notifier,
    bool proceed,
    bool showed_interstitial,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  if (!slow_check_notifier) {
    OnCompleteCheck(false /* slow_check */, proceed, showed_interstitial,
                    performed_check);
    return;
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    slow_check_callback_.Run();
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(slow_check_callback_));
  }

  // In this case |proceed| and |showed_interstitial| should be ignored. The
  // result will be returned by calling |*slow_check_notifier| callback.
  *slow_check_notifier =
      base::BindOnce(&UrlCheckerOnSB::OnCompleteCheck, base::Unretained(this),
                     true /* slow_check */);
}

void UrlCheckerOnSB::OnCompleteCheck(
    bool slow_check,
    bool proceed,
    bool showed_interstitial,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    complete_callback_.Run(slow_check, proceed, showed_interstitial,
                           performed_check);
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(complete_callback_, slow_check, proceed,
                                  showed_interstitial, performed_check));
  }
}

}  // namespace safe_browsing
